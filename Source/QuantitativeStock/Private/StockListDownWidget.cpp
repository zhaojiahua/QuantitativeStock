#include "StockListDownWidget.h"
#include "Components/ScrollBox.h"
#include "StockListDownItemWidget.h"
#include "ItemRightClickWidget.h"
#include "Components/Overlay.h"
#include "Blueprint/DragDropOperation.h"


UStockListDownItemWidget* UStockListDownWidget::GetCurrentSelectedItem(){
	if (currentSelectedItemWidget_) {
		return Cast<UStockListDownItemWidget>(currentSelectedItemWidget_);
	}
	return nullptr;
}

UItemRightClickWidget* UStockListDownWidget::GetRightClickWidget(){
	if (itemRight) {
		return itemRight;
	}
	return nullptr;
}

void UStockListDownWidget::ClearDownListItems(){
	TArray<UWidget*> downlistItems = listScrollBox_->GetAllChildren();
	for (UWidget* item : downlistItems)item->RemoveFromParent();
}

bool UStockListDownWidget::UpdateStockListDatas(const TArray<FQTStockRealTimeData>& listStocksDatas){
	if (stockListDownItemWidget) {
		for (const FQTStockRealTimeData& realTimeData : listStocksDatas) {
			UStockListDownItemWidget* downlistitem = CreateWidget<UStockListDownItemWidget>(GetWorld(), stockListDownItemWidget);
			downlistitem->StockDownListWidget_ = this;
			downlistitem->UpdateStockDatas(realTimeData);
			listScrollBox_->AddChild(downlistitem);
		}
		downListItemNum = listStocksDatas.Num();
		if (itemRightClickWidget) {
			itemRight = CreateWidget<UItemRightClickWidget>(GetWorld(), itemRightClickWidget);
			itemRight->companyNameIndexWidget = companyNameIndexWidget;
			Overlay_forRight->AddChild(itemRight);
			itemRight->SetVisibility(ESlateVisibility::Hidden);
		}
		return true;
	}
	return false;
}

bool UStockListDownWidget::GetCurrentDownListDatas(TArray<FString>& outListStocksDatas_justCode){
	TArray<UWidget*> downlistItems = listScrollBox_->GetAllChildren();
	for (UWidget* item : downlistItems) {
		UStockListDownItemWidget* downlistitem = Cast<UStockListDownItemWidget>(item);
		if (downlistitem) {
			outListStocksDatas_justCode.Add(downlistitem->GetStockDataCode());
		}
		else return false;
	}
	return true;
}

FReply UStockListDownWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent){
	SetAllScrollItemNoScale();
	itemRight->SetHidden();
	itemheight = listScrollBox_->GetChildAt(0)->GetDesiredSize().Y;
	FVector2D screenPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	itemindex = screenPos.Y / itemheight;
	currentSelectedItemWidget_ = listScrollBox_->GetChildAt(itemindex - 1);
	SetAllItemOrgColor();//高亮显示之前全部重置颜色
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && itemindex > 0) {
		if (currentSelectedItemWidget_) {//高亮显示
			currentSelectedItemWidget_->SetRenderScale(FVector2D(1.02f, 1.02f));
			Cast<UStockListDownItemWidget>(currentSelectedItemWidget_)->SetItemLightColor();
		}
		if (itemRight) {
			itemRight->SetRenderTransform(FWidgetTransform(screenPos + FVector2D(-2.0f), FVector2D(1.0f, 1.0f), FVector2D(0.0f, 0.0f), 0.0f));
			itemRight->SetVisibility(ESlateVisibility::Visible);
			itemRight->currentDownListPath = currentDownListPath;
			itemRight->currentItemIndex = itemindex - 1;
			itemRight->ListDownWidget_ = this;
		}
		firstClick = true;
	}
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) {
		if (currentSelectedItemWidget_) {//高亮显示
			currentSelectedItemWidget_->SetRenderScale(FVector2D(1.05f, 1.05f));
			Cast<UStockListDownItemWidget>(currentSelectedItemWidget_)->SetItemLightColor();
		}
		if (!firstClick) startMove = true;
		firstClick = false;
	}
	return FReply::Handled();
}

FReply UStockListDownWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent){
	if (currentSelectedItemWidget_) {
		Cast<UStockListDownItemWidget>(currentSelectedItemWidget_)->SetItemOrgColor();
		SetAllScrollItemNoScale();
		currentSelectedItemWidget_ = nullptr;
	}
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) {
		SetCursor(EMouseCursor::Default);
		startMove = false;
	}
	return FReply::Handled();
}

FReply UStockListDownWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent){
	if (currentSelectedItemWidget_ && startMove) {
		floatindex = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()).Y / itemheight;
		if (floatindex < 1) {
			SetScrollBoxItemIndex(currentSelectedItemWidget_, 0);
			SetCursor(EMouseCursor::Default);
			startMove = false;
			if (currentSelectedItemWidget_) {
				currentSelectedItemWidget_->SetRenderScale(FVector2D(1.0f, 1.0f));
				currentSelectedItemWidget_->SetRenderOpacity(1.0f);
			}
			return FReply::Handled();
		}
		if (floatindex > downListItemNum) {
			SetScrollBoxItemIndex(currentSelectedItemWidget_, downListItemNum);
			SetCursor(EMouseCursor::Default);
			startMove = false;
			if (currentSelectedItemWidget_) {
				currentSelectedItemWidget_->SetRenderScale(FVector2D(1.0f, 1.0f));
				currentSelectedItemWidget_->SetRenderOpacity(1.0f);
			}
			return FReply::Handled();
		}
		SetScrollBoxItemIndex(currentSelectedItemWidget_, floatindex - 1);
	}
	return FReply::Handled();
}

void UStockListDownWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent){
	SetCursor(EMouseCursor::Default);
	firstClick = true;
}

void UStockListDownWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent){
	if (startMove) {
		FGeometry InGeometry = GetPaintSpaceGeometry();
		floatindex = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()).Y / itemheight;
		if (floatindex < 1) {
			SetScrollBoxItemIndex(currentSelectedItemWidget_, 0);
			SetCursor(EMouseCursor::Default);
			startMove = false;
			if (currentSelectedItemWidget_) {
				currentSelectedItemWidget_->SetRenderScale(FVector2D(1.0f, 1.0f));
				currentSelectedItemWidget_->SetRenderOpacity(1.0f);
			}
			currentSelectedItemWidget_ = nullptr;
			SetAllItemOrgColor();
			SetAllScrollItemNoScale();
			return;
		}
		if (floatindex > downListItemNum) {
			SetScrollBoxItemIndex(currentSelectedItemWidget_, downListItemNum);
			SetCursor(EMouseCursor::Default);
			startMove = false;
			if (currentSelectedItemWidget_) {
				currentSelectedItemWidget_->SetRenderScale(FVector2D(1.0f, 1.0f));
				currentSelectedItemWidget_->SetRenderOpacity(1.0f);
			}
			currentSelectedItemWidget_ = nullptr;
			SetAllItemOrgColor();
			SetAllScrollItemNoScale();
			return;
		}
		SetScrollBoxItemIndex(currentSelectedItemWidget_, floatindex - 1);
		SetCursor(EMouseCursor::Default);
		startMove = false;
		if (currentSelectedItemWidget_) {
			currentSelectedItemWidget_->SetRenderScale(FVector2D(1.0f, 1.0f));
			currentSelectedItemWidget_->SetRenderOpacity(1.0f);
		}
	}
	currentSelectedItemWidget_ = nullptr;
	SetAllItemOrgColor();
	SetAllScrollItemNoScale();
}

void UStockListDownWidget::SetAllScrollItemNoScale(){
	for (auto item : listScrollBox_->GetAllChildren()) {
		item->SetRenderScale(FVector2D(1.0f));
	}
}

void UStockListDownWidget::SetScrollBoxItemIndex(UWidget* DroppedWidget, int16 newindex, bool moveData) {
	//调整UI显示顺序的同时调整数据顺序
	if (moveData) {
		FString fileContent;
		FString filename = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/%s"), *currentDownListPath);
		if (!FFileHelper::LoadFileToString(fileContent, *filename)) { UE_LOG(LogTemp, Warning, TEXT("---------->> %s文档加载失败!"), *currentDownListPath); return; }
		TSharedPtr<FJsonObject> rootObj;
		TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(fileContent);
		if (FJsonSerializer::Deserialize(jsonReader, rootObj) && rootObj.IsValid()) {
			TArray< TSharedPtr <FJsonValue>> downlistStocks = rootObj->GetArrayField(TEXT("StockList"));
			int16 oldindex = itemindex - 1;
			if (downlistStocks.IsValidIndex(oldindex) && downlistStocks.IsValidIndex(newindex)) {
				TSharedPtr<FJsonValue> temp = downlistStocks[oldindex];
				downlistStocks.RemoveAt(oldindex);
				downlistStocks.Insert(temp, newindex);
				rootObj->SetNumberField(TEXT("FetchedAt"), FDateTime::Now().GetYear() * 10000 + FDateTime::Now().GetMonth() * 100 + FDateTime::Now().GetDay());
				rootObj->SetArrayField(TEXT("StockList"), downlistStocks);
				FString outputString;
				TSharedRef<TJsonWriter<>> jsonWriter = TJsonWriterFactory<>::Create(&outputString);
				if (FJsonSerializer::Serialize(rootObj.ToSharedRef(), jsonWriter)) {
					if (FFileHelper::SaveStringToFile(outputString, *filename)) {
						UE_LOG(LogTemp, Warning, TEXT("---------->> %s历史数据已成功保存到本地文件: %s"), *currentDownListPath, *filename);
					}
					else { UE_LOG(LogTemp, Error, TEXT("---------->> 保存%s历史数据到本地文件失败: %s"), *currentDownListPath, *filename); return;}
				}
				else { UE_LOG(LogTemp, Error, TEXT("---------->> 序列化%s历史数据失败!"), *currentDownListPath); return;}
			}
		}
		else{ UE_LOG(LogTemp, Warning, TEXT("---------->> %s历史数据反序列化失败!"), *currentDownListPath); return; }
	}
	//调整UI显示顺序
	TArray<UWidget*> downlistItems = listScrollBox_->GetAllChildren();
	downlistItems.Remove(DroppedWidget);
	downlistItems.Insert(DroppedWidget, FMath::Clamp<int16>(newindex, 0, downlistItems.Num()));
	listScrollBox_->ClearChildren();
	for (auto item : downlistItems)listScrollBox_->AddChild(item);
}

void UStockListDownWidget::RemoveScrollBoxItem(int16 removeindex) {
	TArray<UWidget*> downlistItems = listScrollBox_->GetAllChildren();
	if (downlistItems.IsValidIndex(removeindex)) {
		downlistItems[removeindex]->RemoveFromParent();
	}
}

void UStockListDownWidget::DeleteSelectedScrollBoxItem(){
	RemoveScrollBoxItem(itemindex - 1);
}

void UStockListDownWidget::TopSelectedScrollBoxItem(){
	TArray<UWidget*> downlistItems = listScrollBox_->GetAllChildren();
	if (downlistItems.IsValidIndex(itemindex - 1)) {
		UWidget* temp = downlistItems[itemindex - 1];
		downlistItems.RemoveAt(itemindex - 1);
		downlistItems.Insert(temp, 0);
	}
	listScrollBox_->ClearChildren();
	for (auto item : downlistItems)listScrollBox_->AddChild(item);
}

