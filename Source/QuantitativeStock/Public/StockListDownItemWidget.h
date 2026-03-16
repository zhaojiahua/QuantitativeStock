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

	UPROPERTY(BlueprintReadWrite, Category = "QT | Params")
	FQTStockRealTimeData StockRealTimeData_;

	UStockListDownWidget* StockDownListWidget_;//父级下拉列表组件指针,用于在被拖动时更新下拉列表的显示顺序

public:
	// 比较两个ItemWidget的指定列（inIndex）数据，ascending为true时升序，false时降序
	bool CompareTo(const UStockListDownItemWidget& Other, int inIndex, bool ascending) const;

protected:

};
