// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/KeepGoingUpSettingsWidget.h"
#include "KeepGoingUpGameMode.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Widgets/SWidget.h"

namespace
{
	FSlateFontInfo MakeSettingsFont(int32 Size)
	{
		FSlateFontInfo Font;
		Font.Size = Size;
		Font.FontObject = LoadObject<UFont>(nullptr, TEXT("/Game/Game/UI/Font/\u5343\u56FE\u539A\u9ED1\u4F53_Font.\u5343\u56FE\u539A\u9ED1\u4F53_Font"));
		if (!Font.FontObject)
		{
			Font.FontObject = LoadObject<UFont>(nullptr, TEXT("/Game/Game/UI/Font/\u4E5F\u5B57\u5DE5\u5382\u71AC\u591C\u9ED1_Font.\u4E5F\u5B57\u5DE5\u5382\u71AC\u591C\u9ED1_Font"));
		}
		return Font;
	}

	UTextBlock* MakeLabel(UWidgetTree* Tree, const FString& Text, const FSlateFontInfo& Font, const FLinearColor& Color)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Label->SetText(FText::FromString(Text));
		Label->SetFont(Font);
		Label->SetColorAndOpacity(FSlateColor(Color));
		return Label;
	}
}

TSharedRef<SWidget> UKeepGoingUpSettingsWidget::RebuildWidget()
{
	if (!bLayoutBuilt && WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
		bLayoutBuilt = true;
	}
	return Super::RebuildWidget();
}

void UKeepGoingUpSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshValues();
}

void UKeepGoingUpSettingsWidget::RefreshValues()
{
	if (!TileVolumeSlider || !PrankVolumeSlider || !MapLengthBox || !PitHpBox)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (const AKeepGoingUpGameMode* GameMode = World->GetAuthGameMode<AKeepGoingUpGameMode>())
		{
			TileVolumeSlider->SetValue(GameMode->GetTileSoundVolume());
			PrankVolumeSlider->SetValue(GameMode->GetPrankSoundVolume());
			MapLengthBox->SetText(FText::AsNumber(GameMode->GetMapLength()));
			PitHpBox->SetText(FText::AsNumber(GameMode->GetStartPitMaxHp()));
		}
	}
}

void UKeepGoingUpSettingsWidget::BuildLayout()
{

	const FSlateFontInfo TitleFont = MakeSettingsFont(28);
	const FSlateFontInfo BodyFont = MakeSettingsFont(18);
	const FLinearColor TextColor(0.95f, 0.95f, 0.95f, 1.f);

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SettingsCanvas"));
	WidgetTree->RootWidget = Canvas;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SettingsPanel"));
	Panel->SetBrushColor(FLinearColor(0.05f, 0.06f, 0.08f, 0.96f));
	Panel->SetPadding(FMargin(28.f));
	if (UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetSize(FVector2D(560.f, 480.f));
	}

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsColumn"));
	Panel->SetContent(Column);

	auto AddRow = [Column](UWidget* Widget, float TopPad)
	{
		if (UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Widget))
		{
			RowSlot->SetPadding(FMargin(0.f, TopPad, 0.f, 0.f));
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	};

	AddRow(MakeLabel(WidgetTree, TEXT("\u8BBE\u7F6E"), TitleFont, TextColor), 0.f);

	AddRow(MakeLabel(WidgetTree, TEXT("\u5730\u5757\u6362\u8272\u58F0\u97F3"), BodyFont, TextColor), 22.f);
	TileVolumeSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("TileVolume"));
	TileVolumeSlider->SetMinValue(0.f);
	TileVolumeSlider->SetMaxValue(1.f);
	TileVolumeSlider->SetStepSize(0.01f);
	AddRow(TileVolumeSlider, 6.f);

	AddRow(MakeLabel(WidgetTree, TEXT("\u5F39\u5E55\u529F\u80FD\u58F0\u97F3"), BodyFont, TextColor), 18.f);
	PrankVolumeSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("PrankVolume"));
	PrankVolumeSlider->SetMinValue(0.f);
	PrankVolumeSlider->SetMaxValue(1.f);
	PrankVolumeSlider->SetStepSize(0.01f);
	AddRow(PrankVolumeSlider, 6.f);

	auto AddConfirmRow = [&](const FString& Label, const FName& BoxName, const FName& ButtonName, TObjectPtr<UEditableTextBox>& OutBox, UButton*& OutButton)
	{
		AddRow(MakeLabel(WidgetTree, Label, BodyFont, TextColor), 18.f);

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		OutBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), BoxName);
		OutBox->WidgetStyle.TextStyle.Font = BodyFont;
		OutBox->WidgetStyle.TextStyle.ColorAndOpacity = FSlateColor(FLinearColor::White);
		if (UHorizontalBoxSlot* BoxSlot = Row->AddChildToHorizontalBox(OutBox))
		{
			BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			BoxSlot->SetVerticalAlignment(VAlign_Center);
		}

		OutButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
		OutButton->SetContent(MakeLabel(WidgetTree, TEXT("\u786E\u5B9A"), BodyFont, FLinearColor(0.08f, 0.08f, 0.08f, 1.f)));
		USizeBox* ConfirmSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ConfirmSize->SetWidthOverride(110.f);
		ConfirmSize->SetHeightOverride(40.f);
		ConfirmSize->SetContent(OutButton);
		if (UHorizontalBoxSlot* ConfirmSlot = Row->AddChildToHorizontalBox(ConfirmSize))
		{
			ConfirmSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
		}
		AddRow(Row, 6.f);
	};

	UButton* MapLengthConfirm = nullptr;
	UButton* PitHpConfirm = nullptr;
	AddConfirmRow(TEXT("\u5730\u56FE\u957F\u5EA6"), TEXT("MapLengthBox"), TEXT("MapLengthConfirm"), MapLengthBox, MapLengthConfirm);
	AddConfirmRow(TEXT("\u8D77\u70B9\u8840\u91CF"), TEXT("PitHpBox"), TEXT("PitHpConfirm"), PitHpBox, PitHpConfirm);
	MapLengthConfirm->OnClicked.AddDynamic(this, &UKeepGoingUpSettingsWidget::HandleMapLengthConfirmed);
	PitHpConfirm->OnClicked.AddDynamic(this, &UKeepGoingUpSettingsWidget::HandlePitHpConfirmed);

	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseSettings"));
	CloseButton->SetContent(MakeLabel(WidgetTree, TEXT("\u5173\u95ED"), BodyFont, FLinearColor(0.08f, 0.08f, 0.08f, 1.f)));
	CloseButton->OnClicked.AddDynamic(this, &UKeepGoingUpSettingsWidget::HandleCloseClicked);
	USizeBox* CloseSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	CloseSize->SetHeightOverride(42.f);
	CloseSize->SetContent(CloseButton);
	AddRow(CloseSize, 24.f);

	StatusText = MakeLabel(WidgetTree, TEXT(""), BodyFont, FLinearColor(0.65f, 0.85f, 0.7f, 1.f));
	AddRow(StatusText, 10.f);

	if (UWorld* World = GetWorld())
	{
		if (const AKeepGoingUpGameMode* GameMode = World->GetAuthGameMode<AKeepGoingUpGameMode>())
		{
			TileVolumeSlider->SetValue(GameMode->GetTileSoundVolume());
			PrankVolumeSlider->SetValue(GameMode->GetPrankSoundVolume());
			MapLengthBox->SetText(FText::AsNumber(GameMode->GetMapLength()));
			PitHpBox->SetText(FText::AsNumber(GameMode->GetStartPitMaxHp()));
		}
	}

	TileVolumeSlider->OnValueChanged.AddDynamic(this, &UKeepGoingUpSettingsWidget::HandleTileVolumeChanged);
	PrankVolumeSlider->OnValueChanged.AddDynamic(this, &UKeepGoingUpSettingsWidget::HandlePrankVolumeChanged);
	RefreshValues();
}

void UKeepGoingUpSettingsWidget::HandleTileVolumeChanged(float Value)
{
	if (UWorld* World = GetWorld())
	{
		if (AKeepGoingUpGameMode* GameMode = World->GetAuthGameMode<AKeepGoingUpGameMode>())
		{
			GameMode->SetTileSoundVolume(Value);
		}
	}
}

void UKeepGoingUpSettingsWidget::HandlePrankVolumeChanged(float Value)
{
	if (UWorld* World = GetWorld())
	{
		if (AKeepGoingUpGameMode* GameMode = World->GetAuthGameMode<AKeepGoingUpGameMode>())
		{
			GameMode->SetPrankSoundVolume(Value);
		}
	}
}

void UKeepGoingUpSettingsWidget::HandleMapLengthConfirmed()
{
	if (!MapLengthBox)
	{
		return;
	}

	const FString Text = MapLengthBox->GetText().ToString().TrimStartAndEnd();
	if (Text.IsEmpty() || !Text.IsNumeric())
	{
		if (StatusText)
		{
			StatusText->SetText(FText::FromString(TEXT("\u5730\u56FE\u957F\u5EA6\u8BF7\u8F93\u5165\u6570\u5B57")));
		}
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (AKeepGoingUpGameMode* GameMode = World->GetAuthGameMode<AKeepGoingUpGameMode>())
		{
			GameMode->ApplyMapLength(FCString::Atoi(*Text));
			MapLengthBox->SetText(FText::AsNumber(GameMode->GetMapLength()));
			if (StatusText)
			{
				StatusText->SetText(FText::FromString(TEXT("\u5730\u56FE\u957F\u5EA6\u5DF2\u66F4\u65B0")));
			}
		}
	}
}

void UKeepGoingUpSettingsWidget::HandlePitHpConfirmed()
{
	if (!PitHpBox)
	{
		return;
	}

	const FString Text = PitHpBox->GetText().ToString().TrimStartAndEnd();
	if (Text.IsEmpty() || !Text.IsNumeric())
	{
		if (StatusText)
		{
			StatusText->SetText(FText::FromString(TEXT("\u8840\u91CF\u8BF7\u8F93\u5165\u6570\u5B57")));
		}
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (AKeepGoingUpGameMode* GameMode = World->GetAuthGameMode<AKeepGoingUpGameMode>())
		{
			GameMode->ApplyStartPitMaxHp(FCString::Atoi(*Text));
			PitHpBox->SetText(FText::AsNumber(GameMode->GetStartPitMaxHp()));
			if (StatusText)
			{
				StatusText->SetText(FText::FromString(TEXT("\u8D77\u70B9\u8840\u91CF\u5DF2\u66F4\u65B0")));
			}
		}
	}
}

void UKeepGoingUpSettingsWidget::HandleCloseClicked()
{
	if (UWorld* World = GetWorld())
	{
		if (AKeepGoingUpGameMode* GameMode = World->GetAuthGameMode<AKeepGoingUpGameMode>())
		{
			GameMode->CloseSettings();
			return;
		}
	}
	RemoveFromParent();
}
