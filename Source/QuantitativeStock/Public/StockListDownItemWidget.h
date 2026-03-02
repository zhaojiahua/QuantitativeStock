// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QTCurveVectorActor.h"
#include "StockListDownItemWidget.generated.h"

class UStockListDownWidget;
UCLASS()
class QUANTITATIVESTOCK_API UStockListDownItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent, Category = "QT")
	void UpdateStockDatas(const FQTStockRealTimeData& inRealTimedata);
	UFUNCTION(BlueprintImplementableEvent, Category = "QT")
	void SetItemLightColor();
	UFUNCTION(BlueprintImplementableEvent, Category = "QT")
	void SetItemOrgColor();
	UFUNCTION(BlueprintImplementableEvent, Category = "QT")
	FString GetStockDataCode();

	UStockListDownWidget* StockDownListWidget_;//父级下拉列表组件指针,用于在被拖动时更新下拉列表的显示顺序

protected:

};
