// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ReplicationGraph.h"
#include "MyReplicationGraph.generated.h"

enum class EClassRepPolicy : uint8
{
	NotRouted,
	RelevantAllConnections,

	//--------------------------
	// Spatializd routes into the grid node

	Spatialize_Static,		// Used for actors for frequent upodates / updates every frame
	Spatialize_Dynamic,		// For do noeed updates every frame
	Spatialize_Dormancy		// Actors that can either be static opr dynamic determined by their AActor::NetDormancy state
};

/**
 * 
 */
UCLASS(Transient, config+Engine)
class TESTREPLICATIONGRAPH_API UMyReplicationGraph : public UReplicationGraph
{
	GENERATED_BODY()


	// Begin UReplicationGraph Implementation
	virtual void InitGlobalActorClassSettings() override;

	virtual void InitGlobalGraphNodes() override;
	virtual void RouteAddNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& GlobalInfo) override;
	virtual void RouteRemoveNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo) override;
	// end UReplicationGraph

	/** Sets class replication info for a class*/
	void InitClassReplicationInfo(FClassReplicationInfo& Info, UClass* InClass, bool bSpatialize, float ServerMaxTickRate);

	UPROPERTY()
	TArray<UClass*> SpatializedClasses;
	UPROPERTY()
	TArray<UClass*> NonSpatializedClasses;
	UPROPERTY()
	TArray<UClass*> AlwaysRelevantClasses;

	// ---------------------------
	// ReplicationGraph Nodes

	/*
	* This is propable the most Important Node in the replication Graph
	*
	* Cavres the map up into grids and determines if an actor should send network updates
	* to a connection depending on the different pre-defined grids
	*/
	UPROPERTY()
	UReplicationGraphNode_GridSpatialization2D* GridNode;

	UPROPERTY()
	UReplicationGraphNode_ActorList* AlwaysRelevantNode;

protected:

	FORCEINLINE bool IsSpatialized(EClassRepPolicy Mapping)
	{
		return Mapping >= EClassRepPolicy::Spatialize_Static;
	}

	/** Gets the mapping used for the given class */
	EClassRepPolicy GetMappingPolicy(UClass* InClass);

	/** Maps a class to a mapping policy */
	TClassMap<EClassRepPolicy> ClassRepPolicies;

	float GridCellSize = 10000.f;		// The size of one grid cell in the grid Node
	float SpatialBiasX = -150000.f;		// "Min X" for replication
	float SpatialBiasY = -200000.f;		// "Min Y" for replication

	bool bDisableSpatialRebuilding = true;
};
