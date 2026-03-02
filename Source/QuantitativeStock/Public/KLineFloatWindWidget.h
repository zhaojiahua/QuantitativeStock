// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QTCurveVectorActor.h"
#include "KLineFloatWindWidget.generated.h"

/**
 * 
 */
UCLASS()
class QUANTITATIVESTOCK_API UKLineFloatWindWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite, Category = "QT | Params")
	float lineXScale = 1.f;
	UPROPERTY(BlueprintReadWrite, Category = "QT | Params")
	float lineYScale = 1.f;
	UPROPERTY(BlueprintReadWrite, Category = "QT | Params")
	float lineYPosition = 0.0f;
	UPROPERTY(BlueprintReadWrite, Category = "QT | Info")
	FQTStockIndex stockInfoDatas;

protected:
	virtual void NativePreConstruct() override;
};
