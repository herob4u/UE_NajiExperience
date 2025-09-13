

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Chess3D/Public/ChessGame.h"
#include "Engine/DataTable.h"
#include "ChessExperience.generated.h"

template<typename U, typename T>
static U Into(const T& val) = delete;

#define IMPL_INTO_ENUM(FromType, ToType)\
template<>\
ToType Into<ToType, FromType>(const FromType& val) { return static_cast<ToType>(val); }

UENUM(BlueprintType)
namespace EChessPieceType
{
	enum Type : uint8
	{
		King = Chess::PieceId_King,
		Queen = Chess::PieceId_Queen,
		Bishop = Chess::PieceId_Bishop,
		Rook = Chess::PieceId_Rook,
		Knight = Chess::PieceId_Knight,
		Pawn = Chess::PieceId_Pawn,

		COUNT = 6
	};
}
IMPL_INTO_ENUM(Chess::EPieceId, EChessPieceType::Type)

USTRUCT(BlueprintType)
struct FChessBoardVisual : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	uint32 BaseTileWidth = 1;

	UPROPERTY(EditAnywhere)
	uint32 BaseTileHeight = 1;

	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UStaticMesh> BoardSurfaceMesh;

	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UMaterialInstance> SurfaceMaterial;

	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UStaticMesh> BoardBodyMesh;
};

struct FPieceRigidBodyMarker
{
	int32 PieceActorIdx = INDEX_NONE;
};

struct FPieceAnimInfo
{
	FVector2D Initial;
	FVector2D Target;
	float ElapsedSeconds;
};

struct FChessAnimContext
{
	float AnimDurationSeconds = 2.0f;
	float KnockoffForceMultiplier = 1.0f;
	float KnockoffDirectionDither = 0.0f;
};

// Randomly offset direction with some dither delta, normalized.
FVector GetDitheredVector(const FVector& dir, float dither);

// Square ease out function
constexpr float GetEasedAlpha(float t);
FVector2D CalculateAnim(float time, float animDuration, const FVector2D& initial, const FVector2D& target);
FVector2D UpdateAnim(float dt, float animDuration, FPieceAnimInfo& animData, bool& finished);
void TriggerKnockoff(UShapeComponent* knockoffBody, const FVector2D& direction, float multiplier, float dither);
void FinishKnockoff(UShapeComponent* knockoffBody);

struct FChessAnimInstance
{
	uint32 Id;
	Chess::PieceIdx PieceIdx;
	FPieceAnimInfo AnimInfo;
};

struct FChessAnimUpdate
{
	float DeltaTime;
};

struct FChessAnimContext
{
	TArray<FChessAnimInstance> ActiveAnims;
	TArray<FChessAnimInstance> FinishedAnims;
	TSet<uint32> PendingStop;
};

// Calculates and update anim state, returns the indices of anim instances that finished
void UpdateAnimInstances(const FChessAnimUpdate& updateInfo, TArray<FChessAnimInstance>& active, TArray<FChessAnimInstance>& outFinished, TArray<FTransform>& outTransform);
uint32 AddAnim(FChessAnimContext& ctx, Chess::PieceIdx piece, const FVector2D& from, const FVector2D& to);
void StopAnim(FChessAnimContext& ctx, uint32 instance);
void UpdateAnim(FChessAnimContext& ctx, FChessAnimUpdate updateInfo);

struct FChessInstancedMesh
{
	Chess::PieceIdx PieceIdx;
	FTransform Transform;
	EChessPieceType::Type PieceType;
	FPrimitiveInstanceId InstanceId;
};

TArray<int32> CollectUpdatingInstancedMeshes(TArrayView<FChessInstancedMesh> instancedMeshes, TArrayView<FChessAnimInstance> animInstances);


struct FChessPieceInfo
{
	Chess::PieceIdx PieceIdx;
	EChessPieceType::Type PieceType;
	uint32 RendererId;
	uint32 AnimId;
};

void GetPieceTypes(const ChessGame& game, TArrayView<Chess::PieceIdx> pieceIndices, TArray<EChessPieceType::Type>& outTypes);
void SetPieceAnim(FChessPieceInfo& pieceInfo /*animations, animData*/);
void StopPieceAnim(FChessPieceInfo& pieceInfo);
void UpdatePieceAnims(const TArray<uint32>& animIds, TArray<FTransform>& outTransforms);
void UpdatePieceInstances(const TArray<uint32>& rendererId, const TArray<EChessPieceType::Type>& pieceTypes, const TArray<FTransform>& transforms);
TArray<int32> UpdatePieceInstancesQuery(const TArray<FChessPieceInfo>& pieceInfos, const TArray<Chess::PieceIdx>& movedPieces); // Returns the indices of PieceInfos that need their ISMs updated (checks if animated or marked)

UCLASS()
class AChessPieceRenderer : public AActor
{
	GENERATED_BODY()
public:
	using InstanceIds = TStaticArray<TArray<FPrimitiveInstanceId>, EChessPieceType::COUNT>;
	AChessPieceRenderer();

	void SetupMeshes(TArrayView<UStaticMesh*> meshes);
	TArray<FChessInstancedMesh> SetupInstances(TArrayView<FChessPieceInfo> pieces, TArrayView<FTransform> transforms, bool worldSpace);
	void UpdateInstances(const TArray<int32>& indices, TArrayView<FChessInstancedMesh> instancedMeshes, bool worldSpace);

	UPROPERTY(EditDefaultsOnly, meta=(ArraySizeEnum))
	UInstancedStaticMeshComponent* InstancedMeshes[EChessPieceType::COUNT];
};

UCLASS()
class NAJIEXPERIENCE_API AChessGame : public AActor
{
	GENERATED_BODY()
public:
	AChessGame();
	// AActor
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	static void SetupGame(APlayerController* player, AController* ai, ChessGame& game);

	UFUNCTION(BlueprintCallable, Category = "Chess3D")
	void Setup(APlayerController* player, AController* ai);

	UFUNCTION(BlueprintCallable, Category = "Chess3D")
	void SetupTileSizes();

	UFUNCTION(BlueprintCallable, Category = "Chess3D")
	void DebugDrawTiles();
	void GetTileBaseSize(float& outX, float& outY) const;
	UFUNCTION(BlueprintPure, Category = "Chess3D")
	FVector GetTilePosition(int32 tileX, int32 tileY) const;

	UFUNCTION(BlueprintCallable, Category = "Chess3D")
	void SetVisual(FDataTableRowHandle row);
	const FChessBoardVisual& GetVisual() const { return m_Visual; }
	void OnVisualsUpdated(const FChessBoardVisual& newVisuals);

	UFUNCTION(BlueprintCallable, Category = "Chess3D")
	void SetRenderer(AChessPieceRenderer* renderer);
	void SetupPiecesPositions(AChessPieceRenderer* renderer);
	void SetupPieceRenderer(AChessPieceRenderer* renderer);
	void UpdatePiecesPositions(AChessPieceRenderer* renderer);
	void UpdatePiecesRenderer(AChessPieceRenderer& renderer);

	UPROPERTY(EditDefaultsOnly, meta=(ArraySizeEnum))
	UStaticMesh* PieceMeshes[EChessPieceType::COUNT];

public:
	UFUNCTION(BlueprintCallable, Category = "Chess3D")
	static void MoveInstruction(AChessGame* chessGame, int32 x1, int32 y1, int32 x2, int32 y2);

	UFUNCTION(BlueprintCallable, Category = "Chess3D")
	static void KillInstruction(AChessGame* chessGame, int32 x, int32 y);

	UFUNCTION(BlueprintPure, Category="Chess3D")
	static int32 GetUndoCount(AChessGame* chessGame);

	UFUNCTION(BlueprintCallable, Category="Chess3D")
	static void UndoInstruction(AChessGame* chessGame);

private:
	ChessGame m_Game;
	APlayerController* m_PlayerController;
	AController* m_AIController;


	UPROPERTY()
	AChessPieceRenderer* m_Renderer;

	UPROPERTY()
	FChessBoardVisual m_Visual;
	
	UPROPERTY(VisibleAnywhere)
	USceneComponent* m_Root;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* m_BoardSurfaceMesh;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* m_BoardBodyMesh;

	TMap<Chess::PieceIdx, FTransform> m_PieceTransforms;
	AChessPieceRenderer::InstanceIds m_RendererInstanceIds;

	FVector2D m_TilePositions[Chess::BOARD_SIZE][Chess::BOARD_SIZE];
};

UCLASS()
class NAJIEXPERIENCE_API AChessExperience : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AChessExperience();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	
	
};
