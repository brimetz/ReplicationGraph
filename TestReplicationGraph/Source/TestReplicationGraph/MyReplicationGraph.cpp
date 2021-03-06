// Fill out your copyright notice in the Description page of Project Settings.


#include "MyReplicationGraph.h"
#include "UObject/UObjectIterator.h"

void UMyReplicationGraph::InitGlobalActorClassSettings()
{
	Super::InitGlobalActorClassSettings();
	// --------------
	// Assign mapping to classes

	auto SetRule = [&](UClass* InClass, EClassRepPolicy Mapping) {ClassRepPolicies.Set(InClass, Mapping); };
	SetRule(AReplicationGraphDebugActor::StaticClass(), EClassRepPolicy::NotRouted);
	SetRule(AInfo::StaticClass(),						EClassRepPolicy::RelevantAllConnections);
	// ex
	//SetRule(AProjectile::StaticClass(),				EClassRepPolicy::Spatialize_Dynamic);
	//SetRule(ABuilding::StaticClass(),					EClassRepPolicy::Spatialize_Static);

	TArray<UClass*> ReplicatedClasses;
	for (TObjectIterator<UClass> itr; itr; ++itr)
	{
		UClass* Class = *itr;
		AActor* ActorCDO = Cast<AActor>(Class->GetDefaultObject());

		// Do Not Add the actor if it does not replicate
		if (!ActorCDO || !ActorCDO->GetIsReplicated())
			continue;

		// Do Not Add Skel and REINST Classes
		FString ClassName = Class->GetName();
		if (ClassName.StartsWith("SKEL_") || ClassName.StartsWith("REINST_"))
			continue;

		ReplicatedClasses.Add(Class);

		// if we already have mapped it to the policy, don't do it again
		if (ClassRepPolicies.Contains(Class, false))
			continue;

		auto ShouldSpatialize = [](const AActor* Actor)
		{
			return Actor->GetIsReplicated() && (!(Actor->bAlwaysRelevant || Actor->bOnlyRelevantToOwner || Actor->bNetUseOwnerRelevancy));
		};

		UClass* SuperClass = Class->GetSuperClass();
		if (AActor* SuperCDO = Cast<AActor>(SuperClass->GetDefaultObject()))
		{
			if (SuperCDO->GetIsReplicated() == ActorCDO->GetIsReplicated()
				&& SuperCDO->bAlwaysRelevant == ActorCDO->bAlwaysRelevant
				&& SuperCDO->bOnlyRelevantToOwner == ActorCDO->bOnlyRelevantToOwner
				&& SuperCDO->bNetUseOwnerRelevancy == ActorCDO->bNetUseOwnerRelevancy)
			{
				continue;
			}

			if (ShouldSpatialize(ActorCDO) == false && ShouldSpatialize(SuperCDO) == true)
			{
				NonSpatializedClasses.Add(Class);
			}
		}

		if (ShouldSpatialize(ActorCDO) == true)
		{
			SetRule(Class, EClassRepPolicy::Spatialize_Dynamic);
		}
		else if (ActorCDO->bAlwaysRelevant && !ActorCDO->bOnlyRelevantToOwner)
		{
			SetRule(Class, EClassRepPolicy::RelevantAllConnections);
		}
	}

	// ---------------------------------
	// Explicitly set replication info for our classes

	TArray<UClass*> ExplicitlySetClasses;

	auto SetClassInfo = [&](UClass* InClass, FClassReplicationInfo& RepInfo)
	{
		GlobalActorReplicationInfoMap.SetClassInfo(InClass, RepInfo);
		ExplicitlySetClasses.Add(InClass);
	};

	FClassReplicationInfo PawnClassInfo;
	PawnClassInfo.CullDistanceSquared = 300000.f * 300000.f;
	SetClassInfo(APawn::StaticClass(), PawnClassInfo);

	for (UClass* ReplicatedClass : ReplicatedClasses)
	{
		if (ExplicitlySetClasses.FindByPredicate([&](const UClass* InClass) {return ReplicatedClass->IsChildOf(InClass); }) != nullptr)
		{
			continue;
		}

		bool bSpatialize = IsSpatialized(ClassRepPolicies.GetChecked(ReplicatedClass));

		FClassReplicationInfo ClassInfo;
		InitClassReplicationInfo(ClassInfo, ReplicatedClass, bSpatialize, NetDriver->NetServerMaxTickRate);
		GlobalActorReplicationInfoMap.SetClassInfo(ReplicatedClass, ClassInfo);
	}
}

void UMyReplicationGraph::InitGlobalGraphNodes()
{
	PreAllocateRepList(3, 12);
	PreAllocateRepList(6, 12);
	PreAllocateRepList(128, 12);
	PreAllocateRepList(512, 12);

	// ------------------------
	// Create our grid Node

	GridNode = CreateNewNode<UReplicationGraphNode_GridSpatialization2D>();
	GridNode->CellSize = GridCellSize;
	GridNode->SpatialBias = FVector2D(SpatialBiasX, SpatialBiasY);

	if (bDisableSpatialRebuilding == true)
	{
		GridNode->AddSpatialRebuildBlacklistClass(AActor::StaticClass());
	}

	AddGlobalGraphNode(GridNode);

	// --------------------------
	// Create our always relevant Node
	AlwaysRelevantNode = CreateNewNode<UReplicationGraphNode_ActorList>();
	AddGlobalGraphNode(AlwaysRelevantNode);
}

void UMyReplicationGraph::RouteAddNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& GlobalInfo)
{
	EClassRepPolicy MappingPolicy = GetMappingPolicy(ActorInfo.Class);
	switch (MappingPolicy)
	{
		case EClassRepPolicy::Spatialize_Static:
		{
			GridNode->AddActor_Static(ActorInfo, GlobalInfo);
			break;
		}

		case EClassRepPolicy::Spatialize_Dynamic:
		{
			GridNode->AddActor_Dynamic(ActorInfo, GlobalInfo);
			break;
		}

		case EClassRepPolicy::Spatialize_Dormancy:
		{
			GridNode->AddActor_Dormancy(ActorInfo, GlobalInfo);
			break;
		}

		default:
		{
			break;
		}
	}
}

void UMyReplicationGraph::RouteRemoveNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo)
{
	EClassRepPolicy MappingPolicy = GetMappingPolicy(ActorInfo.Class);
	switch (MappingPolicy)
	{
	case EClassRepPolicy::Spatialize_Static:
	{
		GridNode->RemoveActor_Static(ActorInfo);
		break;
	}

	case EClassRepPolicy::Spatialize_Dynamic:
	{
		GridNode->RemoveActor_Dynamic(ActorInfo);
		break;
	}

	case EClassRepPolicy::Spatialize_Dormancy:
	{
		GridNode->RemoveActor_Dormancy(ActorInfo);
		break;
	}

	default:
	{
		break;
	}
	}
}

void UMyReplicationGraph::InitClassReplicationInfo(FClassReplicationInfo& Info, UClass* InClass, bool bSpatialize, float ServerMaxTickRate)
{
	if (AActor* CDO = Cast<AActor>(InClass->GetDefaultObject()))
	{
		if (bSpatialize == true)
		{
			Info.CullDistanceSquared = CDO->NetCullDistanceSquared;
		}

		Info.ReplicationPeriodFrame = FMath::Max<uint32>(FMath::RoundToFloat(ServerMaxTickRate / CDO->NetUpdateFrequency), 1);
	}
}

EClassRepPolicy UMyReplicationGraph::GetMappingPolicy(UClass* InClass)
{

	return ClassRepPolicies.Get(InClass) != NULL ? *ClassRepPolicies.Get(InClass) : EClassRepPolicy::NotRouted;
}
