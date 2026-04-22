// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QTLineChart.generated.h"

/*
 折线统计图表组件
 */
UCLASS()
class QUANTITATIVESTOCK_API UQTLineChart : public UUserWidget
{
	GENERATED_BODY()

public:
	//传入要绘制的点,将这些点归一化处理后放入LineOrgPoints中(调用这个函数就等于绘制了折线图像)(如果drawhorizon=true将绘制一条水平线,水平线的值是drawHorizontalValue,并将计算后的水平线在Canvas中的位置返回出去)
	UFUNCTION(BlueprintCallable, Category = "QT | Chart")
	void SetLineChartDrawElements(const TArray<float>& inPoints, float drawHorizontalValue, float& outHorizonPt,bool drawhorizon = false);
	//传入要绘制的点,将这些点归一化处理后放入BarOrgPoints中(调用这个函数就等于绘制了柱形图像)
	UFUNCTION(BlueprintCallable, Category = "QT | Chart")
	void SetBarChartDrawElements(const TArray<float>& inPoints, float& outZeroHorizon);


protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	//virtual void NativePreConstruct() override;
	//virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	
private:
	//折线图的绘制数据(归一化后的数据,直接用于canvas绘制)
	TArray<float> LineOrgPoints;
	float HorizontalPoint;
	bool DrawHorizon ;

	//柱形图的绘制数据(归一化后的数据,直接用于canvas绘制)
	TArray<float> BarOrgPoints;
	float ZeroHorizon;

	//查找数组中的最值(.X是最大值 .Y是最小值)
	FVector2D GetMaxMinValue(const TArray<float>& invalues);
};
