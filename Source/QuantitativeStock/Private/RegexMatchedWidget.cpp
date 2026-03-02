
#include "RegexMatchedWidget.h"
#include "Components/ScrollBox.h"
#include "RegexMatchedItemWidget.h"

void URegexMatchedWidget::UpdateMatchedDatas(const TArray<TSharedPtr<FQTStockListRow>>& matchedList){
	if (ItemsScrollBox) {
		TArray<UWidget*> downlistItems = ItemsScrollBox->GetAllChildren();
		if (downlistItems.Num() > 0 && RegexMatchedItemWidget) {
			for (UWidget* item : downlistItems)item->RemoveFromParent();
			for (const auto& simpleData : matchedList) {
				URegexMatchedItemWidget* matcheditem = CreateWidget<URegexMatchedItemWidget>(GetWorld(), RegexMatchedItemWidget);
				matcheditem->UpdateMatchedDatas(*simpleData);
				matcheditem->CompanyNameWidget = CompanyNameWidget;
				ItemsScrollBox->AddChild(matcheditem);
			}
		}
	}
}
