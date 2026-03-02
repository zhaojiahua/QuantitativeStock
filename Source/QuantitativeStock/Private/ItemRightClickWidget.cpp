
#include "ItemRightClickWidget.h"

FReply UItemRightClickWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent){
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) {
		SetVisibility(ESlateVisibility::Hidden);
	}
	return FReply::Handled();
}

FReply UItemRightClickWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent){

	return FReply::Handled();
}
