// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QTCurveVectorActor.h"
#include "CompanyNameIndexWidget.h"
#include "RegexMatchedItemWidget.generated.h"

/**
 * 
 */
UCLASS()
class QUANTITATIVESTOCK_API URegexMatchedItemWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite, Category = "QT | Params")
	UCompanyNameIndexWidget* CompanyNameWidget;

	UFUNCTION(BlueprintImplementableEvent, Category = "QT")
	void UpdateMatchedDatas(const FQTStockListRow& inRealTimedata);
};
