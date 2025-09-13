


#include "ChessExperience.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/ShapeComponent.h"

#include "Containers/BitArray.h"

AChessPieceRenderer::AChessPieceRenderer()
{
	PrimaryActorTick.bCanEverTick = false;

	for (uint8 pieceId = 0; pieceId < EChessPieceType::COUNT; ++pieceId)
	{
		FName nameId(FString::Printf(TEXT("InstancedMesh_%i"), pieceId));
		InstancedMeshes[pieceId] = CreateDefaultSubobject<UInstancedStaticMeshComponent>(nameId);
	}
}

void AChessPieceRenderer::SetupMeshes(TArrayView<UStaticMesh*> meshes)
{
	verify(meshes.Num() == EChessPieceType::COUNT);
	for (uint8 pieceId = 0; pieceId < EChessPieceType::COUNT; ++pieceId)
	{
		InstancedMeshes[pieceId]->SetStaticMesh(meshes[pieceId]);
	}
}

TArray<FChessInstancedMesh> AChessPieceRenderer::SetupInstances(TArrayView<FChessPieceInfo> pieces, TArrayView<FTransform> transforms, bool worldSpace)
{
	check(pieces.Num() == transforms.Num());

	TArray<FChessInstancedMesh> result;
	result.Reserve(pieces.Num());

	for (uint8 pieceId = 0; pieceId < EChessPieceType::COUNT; ++pieceId)
	{
		InstancedMeshes[pieceId]->ClearInstances();
	}

	for (int32 i = 0; i < pieces.Num(); ++i)
	{
		FChessInstancedMesh instanceInfo;
		instanceInfo.PieceIdx = pieces[i].PieceIdx;
		instanceInfo.PieceType = pieces[i].PieceType;
		instanceInfo.Transform = transforms[i];

		instanceInfo.InstanceId = InstancedMeshes[instanceInfo.PieceType]->AddInstanceById(transforms[i], worldSpace);

		result.Add(MoveTemp(instanceInfo));
	}

	return result;
}

AChessPieceRenderer::InstanceIds AChessPieceRenderer::SetupInstances(TArrayView<TArray<FTransform>> instances, bool worldSpace)
{
	verify(instances.Num() == EChessPieceType::COUNT);

	InstanceIds ids;

	for (uint8 pieceId = 0; pieceId < EChessPieceType::COUNT; ++pieceId)
	{
		InstancedMeshes[pieceId]->ClearInstances();
		ids[pieceId] = InstancedMeshes[pieceId]->AddInstancesById(instances[pieceId], worldSpace, false);
	}

	return ids;
}

/*
void AChessPieceRenderer::UpdateInstances(EChessPieceType::Type pieceId, TArrayView<FPrimitiveInstanceId> instanceIds, TArrayView<FTransform> instances, bool worldSpace)
{
	auto& instancedMesh = InstancedMeshes[pieceId];

	for(int32 i = 0; i < instanceIds.Num(); ++i)
		instancedMesh->UpdateInstanceTransformById(instanceIds[i], instances[i], worldSpace);
}
*/

void AChessPieceRenderer::UpdateInstances(const TArray<int32>& indices, TArrayView<FChessInstancedMesh> instancedMeshes, bool worldSpace)
{
	for (int32 idx : indices)
	{
		EChessPieceType::Type pieceId = instancedMeshes[idx].PieceType;
		FPrimitiveInstanceId instanceId = instancedMeshes[idx].InstanceId;

		InstancedMeshes[pieceId]->UpdateInstanceTransformById(instanceId, instancedMeshes[idx].Transform, worldSpace);
	}
}

AChessGame::AChessGame()
{
	m_Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	m_BoardSurfaceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardSurfaceMesh"));
	m_BoardBodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardBodyMesh"));

	m_BoardSurfaceMesh->SetMobility(EComponentMobility::Movable);
	m_BoardBodyMesh->SetMobility(EComponentMobility::Movable);


	SetRootComponent(m_Root);
	m_BoardSurfaceMesh->SetupAttachment(m_Root);
	m_BoardBodyMesh->SetupAttachment(m_Root);
}

void AChessGame::Setup(APlayerController* player, AController* ai)
{
	SetupGame(player, ai, m_Game);
	m_PlayerController = player;
	m_AIController = ai;

	UpdatePiecesPositions(m_Renderer);
}

void AChessGame::SetupTileSizes()
{
	float tileWidth = 1.0f;
	float tileDepth = 1.0f;

	GetTileBaseSize(tileWidth, tileDepth);
	float boardWidth = tileWidth * Chess::BOARD_SIZE;
	float boardDepth = tileDepth * Chess::BOARD_SIZE;

	const float scale = GetActorScale().GetMax();
	tileWidth *= scale;
	tileDepth *= scale;
	boardWidth *= scale;
	boardDepth *= scale;

	FVector2D origin;
	origin.X = GetActorLocation().X - (boardWidth * 0.5f) + (tileWidth * 0.5f);
	origin.Y = GetActorLocation().Y - (boardDepth * 0.5f) + (tileDepth * 0.5f);

	for (int32 x = 0; x < Chess::BOARD_SIZE; ++x)
	{
		for (int32 y = 0; y < Chess::BOARD_SIZE; ++y)
		{
			m_TilePositions[x][y].X = origin.X + (tileWidth * x);
			m_TilePositions[x][y].Y = origin.Y + (tileDepth * y);
		}
	}
}

void AChessGame::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName().IsEqual(TEXT("RelativeScale3D")))
	{
		UE_LOG(LogTemp, Log, TEXT("Scale changed!"));
	}
}

void AChessGame::SetupGame(APlayerController* player, AController* ai, ChessGame& game)
{
	Chess::PlayerJoinInfo playerInfo;
	playerInfo.Controller = player;
	playerInfo.SelectedFaction = ChessGame::WHITE;

	Chess::PlayerJoinInfo aiInfo;
	aiInfo.Controller = ai;
	aiInfo.SelectedFaction = ChessGame::BLACK;

	game.SetupGame(playerInfo, aiInfo);
}

void AChessGame::DebugDrawTiles()
{
	FVector3f extent;
	extent.Z = 1.0f;
	extent *= GetActorScale().GetMax();

	GetTileBaseSize(extent.X, extent.Y);
	extent.X *= 0.5f;
	extent.Y *= 0.5f;
	for (int32 x = 0; x < Chess::BOARD_SIZE; ++x)
	{
		for (int32 y = 0; y < Chess::BOARD_SIZE; ++y)
		{
			DrawDebugPoint(GetWorld(), GetTilePosition(x, y), 6.0F, FColor::Red);
			DrawDebugBox(GetWorld(), GetTilePosition(x,y), FVector(extent), FColor::Green);
		}
	}
}

void AChessGame::GetTileBaseSize(float& outX, float& outY) const
{
	outX = GetVisual().BaseTileHeight;
	outY = GetVisual().BaseTileWidth;
}

FVector AChessGame::GetTilePosition(int32 tileX, int32 tileY) const
{
	return FVector(m_TilePositions[tileX][tileY], GetActorLocation().Z);
}

void AChessGame::SetVisual(FDataTableRowHandle row)
{
	if (!row.DataTable)
		return;

	if (row.DataTable.Get()->RowStruct != FChessBoardVisual::StaticStruct())
		return;

	m_Visual = *(FChessBoardVisual*)row.DataTable->FindRowUnchecked(row.RowName);
	OnVisualsUpdated(m_Visual);
}

void AChessGame::OnVisualsUpdated(const FChessBoardVisual& newVisuals)
{
	m_BoardBodyMesh->SetStaticMesh(newVisuals.BoardBodyMesh.LoadSynchronous());
	m_BoardSurfaceMesh->SetStaticMesh(newVisuals.BoardSurfaceMesh.LoadSynchronous());
	m_BoardSurfaceMesh->SetMaterial(0, newVisuals.SurfaceMaterial.LoadSynchronous());

	SetupTileSizes();
	UpdatePiecesPositions(m_Renderer);
}

void AChessGame::SetRenderer(AChessPieceRenderer* renderer)
{
	if (renderer != m_Renderer)
	{
		m_Renderer = renderer;
		SetupPieceRenderer(m_Renderer);
	}
}

void AChessGame::SetupPiecesPositions(AChessPieceRenderer* renderer)
{
	const Chess::Board& board = m_Game.GetBoard();
	for (int32 x = 0; x < Chess::BOARD_SIZE; ++x)
	{
		for (int32 y = 0; y < Chess::BOARD_SIZE; ++y)
		{
			Chess::PieceIdx idx = board.At(x, y);
			if (idx == Chess::PIECE_IDX_NONE)
				continue;

			FTransform& transform = m_PieceTransforms.FindOrAdd(idx);
			const FVector pos = GetTilePosition(x, y);
			transform.SetTranslation(pos);
		}
	}
}

void AChessGame::SetupPieceRenderer(AChessPieceRenderer* renderer)
{
	if (m_Renderer)
	{
		m_Renderer->SetupMeshes(TArrayView<UStaticMesh*>(PieceMeshes));

		TArray<FTransform> pieceTransforms[EChessPieceType::COUNT];
		for (const auto pair : m_PieceTransforms)
		{
			EChessPieceType::Type pieceId = static_cast<EChessPieceType::Type>(m_Game.GetPieceType(pair.Key));
			pieceTransforms[pieceId].Add(pair.Value);
		}

		m_RendererInstanceIds = m_Renderer->SetupInstances(pieceTransforms, true);
	}
	else
	{
		for (auto& instances : m_RendererInstanceIds)
			instances.Empty();
	}
}

void AChessGame::UpdatePiecesPositions(AChessPieceRenderer* renderer)
{
	const Chess::Board& board = m_Game.GetBoard();
	for (int32 x = 0; x < Chess::BOARD_SIZE; ++x)
	{
		for (int32 y = 0; y < Chess::BOARD_SIZE; ++y)
		{
			Chess::PieceIdx idx = board.At(x, y);
			if (idx == Chess::PIECE_IDX_NONE)
				continue;

			FTransform& transform = m_PieceTransforms.FindOrAdd(idx);
			const FVector pos = GetTilePosition(x, y);
			transform.SetTranslation(pos);
		}
	}

	if (renderer)
	{
		UpdatePiecesRenderer(*renderer);
	}
}

void AChessGame::UpdatePiecesRenderer(AChessPieceRenderer& renderer)
{
	TArray<FTransform> pieceTransforms[EChessPieceType::COUNT];
	for (const auto pair : m_PieceTransforms)
	{
		EChessPieceType::Type pieceId = Into<EChessPieceType::Type>(m_Game.GetPieceType(pair.Key));
		pieceTransforms[pieceId].Add(pair.Value);
	}

	for (uint8 pieceId = 0; pieceId < EChessPieceType::COUNT; ++pieceId)
	{
		renderer.UpdateInstances(static_cast<EChessPieceType::Type>(pieceId), instanceIds, pieceTransforms[pieceId], true);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug Instructions
void AChessGame::MoveInstruction(AChessGame* chessGame, int32 x1, int32 y1, int32 x2, int32 y2)
{
	Chess::FMoveTileCmd moveCmd;
	moveCmd.From.X = x1;
	moveCmd.From.Y = y1;
	moveCmd.To.X = x2;
	moveCmd.To.Y = y2;
	moveCmd.ResolutionHint = Chess::EMoveResolution::MOVE;

	chessGame->m_Game.EvaluateInstruction(Chess::FBoardInstruction(TInPlaceType<Chess::FMoveTileCmd>(), MoveTemp(moveCmd)));

	chessGame->UpdatePiecesPositions(chessGame->m_Renderer);
}

void AChessGame::KillInstruction(AChessGame* chessGame, int32 x, int32 y)
{
	Chess::FKillCmd killCmd;
	killCmd.X = x;
	killCmd.Y = y;

	chessGame->m_Game.EvaluateInstruction(Chess::FBoardInstruction(TInPlaceType<Chess::FKillCmd>(), MoveTemp(killCmd)));

	chessGame->UpdatePiecesPositions(chessGame->m_Renderer);
}

int32 AChessGame::GetUndoCount(AChessGame* chessGame)
{
	if(chessGame)
		return chessGame->m_Game.GetHistory().Num();

	return 0;
}

void AChessGame::UndoInstruction(AChessGame* chessGame)
{
	if (chessGame)
	{
		chessGame->m_Game.UndoInstruction();
		chessGame->UpdatePiecesPositions(chessGame->m_Renderer);
	}
}


// Sets default values
AChessExperience::AChessExperience()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AChessExperience::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AChessExperience::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FVector GetDitheredVector(const FVector& dir, float dither)
{
	return (dir + (dither* FMath::VRand())).GetSafeNormal();
}

constexpr float GetEasedAlpha(float t)
{
	return -((t - 1) * (t - 1)) + 1;
}

FVector2D CalculateAnim(float time, float animDuration, const FVector2D& initial, const FVector2D& target)
{
	if (animDuration <= 0.0f)
	{
		verify(false);
		return initial;
	}

	if (time > animDuration)
		return target;

	float t = (time) / animDuration;
	t = GetEasedAlpha(t);

	FVector2D result = FMath::Lerp(initial, target, t);
	
	return result;
}

FVector2D UpdateAnim(float dt, float animDuration, FPieceAnimInfo& animData, bool& finished)
{
	float time = animData.ElapsedSeconds + dt;
	if (time >= animData.ElapsedSeconds)
		finished = true;

	animData.ElapsedSeconds = time;
	return CalculateAnim(time, animDuration, animData.Initial, animData.Target);
}

void TriggerKnockoff(UShapeComponent* knockoffBody, const FVector2D& direction, float multiplier, float dither)
{
	if (knockoffBody)
	{
		FVector ditheredImpulse = GetDitheredVector(FVector(direction, 0.0f), dither);
		knockoffBody->SetSimulatePhysics(true);
		knockoffBody->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
		knockoffBody->AddImpulse(ditheredImpulse);
	}
}

void FinishKnockoff(UShapeComponent* knockoffBody)
{
	if (knockoffBody)
	{
		knockoffBody->SetSimulatePhysics(false);
		knockoffBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void UpdateAnimInstances(const FChessAnimUpdate& updateInfo, TArray<FChessAnimInstance>& active, TArray<FChessAnimInstance>& outFinished, TArray<FTransform>& outTransform)
{
	constexpr float animSpeed = 2.0f;
	outTransform.Reserve(active.Num());
		
	TBitArray finishedArr(false, active.Num());
	for (int32 i = 0; i < active.Num(); ++i)
	{
		bool finished = false;
		UpdateAnim(updateInfo.DeltaTime, animSpeed, active[i].AnimInfo, finished);
		finishedArr[i] = finished;
	}

	outFinished.Reserve(finishedArr.CountSetBits());
}

uint32 AddAnim(FChessAnimContext& ctx, Chess::PieceIdx piece, const FVector2D& from, const FVector2D& to)
{
	return uint32();
}

void StopAnim(FChessAnimContext& ctx, uint32 instance)
{
}

void UpdateAnim(FChessAnimContext& ctx, FChessAnimUpdate updateInfo)
{
	ctx.FinishedAnims.Empty(ctx.FinishedAnims.Num());

	// Process stops
	for (int32 i = 0; i < ctx.ActiveAnims.Num(); ++i)
	{
		FChessAnimInstance animInstance = ctx.ActiveAnims[i];
		if (ctx.PendingStop.Contains(animInstance.Id))
		{
			ctx.ActiveAnims.RemoveAtSwap(i, EAllowShrinking::No);
			ctx.FinishedAnims.Add(MoveTemp(animInstance));
		}
	}

	ctx.ActiveAnims.Shrink();

	// Calculate anims and get the resulting transforms
	TArray<FTransform> transforms;
	UpdateAnimInstances(updateInfo, ctx.ActiveAnims, ctx.FinishedAnims, transforms);
}

TArray<int32> CollectUpdatingInstancedMeshes(TArrayView<FChessInstancedMesh> instancedMeshes, TArrayView<FChessAnimInstance> animInstances)
{
	TSet<Chess::PieceIdx> updatingPieces;
	for (const FChessAnimInstance& animInstance : animInstances)
	{
		updatingPieces.Add(animInstance.PieceIdx);
	}

	TArray<int32> indices;
	indices.Reserve(updatingPieces.Num());
	for (int32 i = 0; i < instancedMeshes.Num(); ++i)
	{
		if (updatingPieces.Contains(instancedMeshes[i].PieceIdx))
			indices.Add(i);
	}

	verify(Algo::IsSorted(indices));

	return indices;
}

void GetPieceTypes(const ChessGame& game, TArrayView<Chess::PieceIdx> pieceIndices, TArray<EChessPieceType::Type>& outTypes)
{
	Algo::Transform(pieceIndices, outTypes, [&game](Chess::PieceIdx pieceIdx) { return Into<EChessPieceType::Type>(game.GetPieceType(pieceIdx)); });
}
