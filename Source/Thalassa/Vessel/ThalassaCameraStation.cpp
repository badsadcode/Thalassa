#include "Vessel/ThalassaCameraStation.h"

#include "CanvasItem.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Canvas.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/App.h"
#include "Net/UnrealNetwork.h"
#include "TextureResource.h"
#include "Vessel/ThalassaVessel.h"

namespace ThalassaCameraStation
{
	constexpr int32 FeedWidth = 480;
	constexpr int32 FeedHeight = 270;

	// The camera on the big screen updates often. The others share a slower round, one at a time.
	constexpr float SelectedSeconds = 1.f / 15.f;
	constexpr float OthersSeconds = 1.f / 12.f;

	// After the last screen or HUD asked for pictures, keep capturing this long.
	constexpr double RequestHoldSeconds = 0.5;
}

void UThalassaCameraStationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UThalassaCameraStationComponent, Selected);
}

void UThalassaCameraStationComponent::Select(const APawn* From, int32 Step)
{
	if (IsOccupant(From) && Cameras.Num() > 0)
	{
		Selected = (Selected + Step % Cameras.Num() + Cameras.Num()) % Cameras.Num();
	}
}

void UThalassaCameraStationComponent::RequestFeeds()
{
	LastRequestTime = GetWorld()->GetTimeSeconds();
}

void UThalassaCameraStationComponent::CreateFeeds()
{
	bFeedsCreated = true;

	// Tests run without a renderer, and a dedicated server has nobody to show pictures to.
	if (GetNetMode() == NM_DedicatedServer || !FApp::CanEverRender())
	{
		return;
	}

	using namespace ThalassaCameraStation;
	for (USceneCaptureComponent2D* Camera : Cameras)
	{
		UTextureRenderTarget2D* Feed = NewObject<UTextureRenderTarget2D>(this);
		Feed->RenderTargetFormat = RTF_RGBA8;
		Feed->ClearColor = FLinearColor::Black;
		Feed->InitAutoFormat(FeedWidth, FeedHeight);
		Feed->UpdateResourceImmediate(true);
		Feeds.Add(Feed);
		if (Camera)
		{
			Camera->TextureTarget = Feed;
		}
	}
}

void UThalassaCameraStationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bFeedsCreated)
	{
		CreateFeeds();
	}
	if (Feeds.Num() == 0 || GetWorld()->GetTimeSeconds() - LastRequestTime > ThalassaCameraStation::RequestHoldSeconds)
	{
		return; // Nobody here is looking. Rendering cameras nobody sees would be wasted work.
	}

	using namespace ThalassaCameraStation;
	SelectedTimer -= DeltaTime;
	if (SelectedTimer <= 0.f && Cameras.IsValidIndex(Selected) && Cameras[Selected])
	{
		SelectedTimer = SelectedSeconds;
		Cameras[Selected]->CaptureScene();
	}

	OthersTimer -= DeltaTime;
	if (OthersTimer <= 0.f && Cameras.Num() > 1)
	{
		OthersTimer = OthersSeconds;
		NextOther = (NextOther + 1) % Cameras.Num();
		if (NextOther == Selected)
		{
			NextOther = (NextOther + 1) % Cameras.Num();
		}
		if (Cameras[NextOther])
		{
			Cameras[NextOther]->CaptureScene();
		}
	}
}

namespace ThalassaCameraDisplay
{
	using namespace ThalassaScreenDraw;

	void DrawFeed(UCanvas* Canvas, const FVector2D& TopLeft, const FVector2D& Size, const UThalassaCameraStationComponent& Station, int32 Index, bool bHighlight)
	{
		const float Thin = FMath::Max(1.f, Size.X / 300.f);
		const float TextScale = FMath::Max(0.6f, Size.X / 500.f);
		const float LabelHeight = 16.f * TextScale;

		// The picture keeps its 16:9 shape inside the space it is given, above its label.
		const FVector2D Room(Size.X, Size.Y - LabelHeight * 1.4f);
		const FVector2D Picture = Room.X / Room.Y > 16.f / 9.f ? FVector2D(Room.Y * 16.f / 9.f, Room.Y) : FVector2D(Room.X, Room.X * 9.f / 16.f);
		const FVector2D PictureAt = TopLeft + FVector2D((Size.X - Picture.X) * 0.5f, 0.f);

		const UTextureRenderTarget2D* Feed = Station.GetFeed(Index);
		if (Feed && Feed->GetResource())
		{
			FCanvasTileItem Tile(PictureAt, Feed->GetResource(), Picture, FLinearColor::White);
			Tile.BlendMode = SE_BLEND_Opaque;
			Canvas->DrawItem(Tile);
		}
		else
		{
			Fill(Canvas, PictureAt, Picture, FLinearColor(0.02f, 0.03f, 0.04f));
			Text(Canvas, TEXT("NO SIGNAL"), PictureAt + Picture * 0.5f - FVector2D(40.f * TextScale, 8.f * TextScale), Dim(Cyan, 0.7f), TextScale);
		}

		Frame(Canvas, PictureAt, Picture, bHighlight ? Amber : Dim(Cyan, 0.6f), bHighlight ? Thin * 2.f : Thin);
		Text(Canvas, FString::Printf(TEXT("CAM %d  %s"), Index + 1, *Station.GetLabel(Index)),
			FVector2D(PictureAt.X, PictureAt.Y + Picture.Y + LabelHeight * 0.2f), bHighlight ? Amber : Cyan, TextScale);
	}

	void DrawGrid(UCanvas* Canvas, const FVector2D& TopLeft, const FVector2D& Size, const UThalassaCameraStationComponent& Station)
	{
		const int32 Count = Station.GetCameraCount();
		if (Count == 0)
		{
			return;
		}
		const int32 Columns = Count > 4 ? 3 : 2;
		const int32 Rows = (Count + Columns - 1) / Columns;
		const FVector2D Cell(Size.X / Columns, Size.Y / Rows);
		const FVector2D Gap = Cell * 0.04f;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector2D At = TopLeft + FVector2D(Cell.X * (Index % Columns), Cell.Y * (Index / Columns)) + Gap;
			DrawFeed(Canvas, At, Cell - Gap * 2.f, Station, Index, Index == Station.GetSelected());
		}
	}
}

void UThalassaCameraScreenComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// While this screen is on someone's screen, the cameras keep sending pictures.
	const AThalassaVessel* Vessel = GetVessel();
	if (Vessel && Vessel->GetCameraStation() && WasRecentlyRendered(0.5f))
	{
		Vessel->GetCameraStation()->RequestFeeds();
	}
}

void UThalassaCameraScreenComponent::DrawContent(UCanvas* Canvas, float Size, const AThalassaVessel& Vessel)
{
	const UThalassaCameraStationComponent* Station = Vessel.GetCameraStation();
	if (!Station)
	{
		return;
	}

	using namespace ThalassaScreenDraw;
	Fill(Canvas, FVector2D::ZeroVector, FVector2D(Size), FLinearColor(0.01f, 0.02f, 0.03f));
	const float Margin = Size * 0.04f;
	const float TextScale = Size / 400.f;
	Text(Canvas, bShowsAll ? TEXT("HULL CAMERAS") : TEXT("CAMERA"), FVector2D(Margin), Cyan, TextScale * 1.2f);

	const FVector2D Area(Size - Margin * 2.f, Size - Margin * 4.f);
	const FVector2D AreaAt(Margin, Margin * 3.f);
	if (bShowsAll)
	{
		ThalassaCameraDisplay::DrawGrid(Canvas, AreaAt, Area, *Station);
	}
	else
	{
		ThalassaCameraDisplay::DrawFeed(Canvas, AreaAt + FVector2D(0.f, Area.Y * 0.15f), FVector2D(Area.X, Area.Y * 0.7f), *Station, Station->GetSelected(), false);
	}
}
