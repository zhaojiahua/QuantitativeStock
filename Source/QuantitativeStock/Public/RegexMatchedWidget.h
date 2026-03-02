// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QTCurveVectorActor.h"
#include "CompanyNameIndexWidget.h"
#include "RegexMatchedWidget.generated.h"


UCLASS()
class QUANTITATIVESTOCK_API URegexMatchedWidget : public UUserWidget
{
	GENERATED_BODY()
	
	public:
		UPROPERTY(meta = (BindWidget))
		class UScrollBox* ItemsScrollBox;
		UPROPERTY(EditDefaultsOnly, Category = "QT | Params")
		TSubclassOf<UUserWidget> RegexMatchedItemWidget;
		UPROPERTY(EditInstanceOnly, Category = "QT | Params")
		UCompanyNameIndexWidget* CompanyNameWidget;

		void UpdateMatchedDatas(const TArray<TSharedPtr<FQTStockListRow>>& matchedList);
};
