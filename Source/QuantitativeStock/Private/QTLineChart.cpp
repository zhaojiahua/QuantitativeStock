// Fill out your copyright notice in the Description page of Project Settings.


#include "QTLineChart.h"

void UQTLineChart::SetLineChartDrawElements(const TArray<float>& inPoints, float drawHorizontalValue, float& outHorizonPt, bool drawhorizon){
	if (inPoints.Num() < 2)return;
	BarOrgPoints.Empty();//先将其他图标类型的数据清空
	FVector2D maxminvalue = GetMaxMinValue(inPoints);
	float minvalue = maxminvalue.Y;
	float maxvalue = maxminvalue.X;
	if (drawhorizon) {
		if (drawHorizontalValue < minvalue)minvalue = drawHorizontalValue;
		if (drawHorizontalValue > maxvalue)maxvalue = drawHorizontalValue;
	}
	LineOrgPoints.Empty();
	LineOrgPoints.Reserve(inPoints.Num() + 2);
	float valueLen = maxvalue - minvalue;
	for (float ptvalue : inPoints) {
		LineOrgPoints.Add(1.0f - (ptvalue - minvalue) / valueLen);
	}
	if (drawhorizon) {
		HorizontalPoint = 1.0f - (drawHorizontalValue - minvalue) / valueLen;//Y轴向翻转
		outHorizonPt = HorizontalPoint;
	}
	DrawHorizon = drawhorizon;
}

void UQTLineChart::SetBarChartDrawElements(const TArray<float>& inPoints, float& outZeroHorizon) {
	if (inPoints.Num() < 2)return;
	LineOrgPoints.Empty();//先将其他图标类型的数据清空
	FVector2D maxminvalue = GetMaxMinValue(inPoints);//获取最值
	if (maxminvalue.X < 0)maxminvalue.X = 0.0;
	if (maxminvalue.Y > 0)maxminvalue.Y = 0.0;
	BarOrgPoints.Empty();
	BarOrgPoints.Reserve(inPoints.Num() + 2);
	float valueLen = maxminvalue.X - maxminvalue.Y;
	ZeroHorizon = 1.0f - (0.0f - maxminvalue.Y) / valueLen;
	outZeroHorizon = ZeroHorizon;
	for (float ptvalue : inPoints) {
		BarOrgPoints.Add(1.0f - (ptvalue - maxminvalue.Y) / valueLen);
	}
}

int32 UQTLineChart::NativePaint(const FPaintArgs& Args,
													const FGeometry& AllottedGeometry, 
													const FSlateRect& MyCullingRect, 
													FSlateWindowElementList& OutDrawElements, 
													int32 LayerId, 
													const FWidgetStyle& InWidgetStyle, 
													bool bParentEnabled) const{
	FDeprecateSlateVector2D localSize = AllottedGeometry.GetLocalSize();
	//FDeprecateSlateVector2D absSize = AllottedGeometry.GetAbsoluteSize();
	//绘制折线图
	if (LineOrgPoints.Num() > 0) {
		TArray<FVector2f> linepoints;
		linepoints.Reserve(LineOrgPoints.Num() + 2);
		for (int i = 0; i < LineOrgPoints.Num(); ++i) {
			linepoints.Add(FVector2f(static_cast<float>(i) / static_cast<float>(LineOrgPoints.Num()) * localSize.X, LineOrgPoints[i] * localSize.Y));
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), linepoints, ESlateDrawEffect::None, FLinearColor(0.8f, 1.0f, 0.4f), true, 4.0f);
		if (DrawHorizon) {
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), { FVector2f(0.0f,HorizontalPoint * localSize.Y) ,FVector2f(localSize.X,HorizontalPoint * localSize.Y) }, ESlateDrawEffect::None, FLinearColor(1.0f, 0.1f, 0.2f), true, 2.0f);
		}
	}
	//绘制柱形图
	if (BarOrgPoints.Num() > 0) {
		float lineWidth = 0.8f * localSize.X / static_cast<float>(BarOrgPoints.Num());
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), { FVector2f(0.0f,ZeroHorizon * localSize.Y) ,FVector2f(localSize.X,ZeroHorizon * localSize.Y) }, ESlateDrawEffect::None, FLinearColor(1.0f, 0.1f, 0.2f), true, 2.0f);
		for (int i = 0; i < BarOrgPoints.Num(); ++i) {
			FLinearColor barColor = BarOrgPoints[i] < ZeroHorizon ? FLinearColor(1.0f, 0.2f, 0.1f) : FLinearColor(0.2f, 1.0f, 0.1f);
			TArray<FVector2f> tempPoints;
			tempPoints.Reserve(2);
			float xpos = (i + 0.5f) / static_cast<float>(BarOrgPoints.Num()) * localSize.X;
			tempPoints.Add(FVector2f(xpos, ZeroHorizon * localSize.Y));
			tempPoints.Add(FVector2f(xpos, BarOrgPoints[i] * localSize.Y));
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), tempPoints, ESlateDrawEffect::None, barColor, true, lineWidth);
		}
	}

	return int32();
}

FVector2D UQTLineChart::GetMaxMinValue(const TArray<float>& invalues){
	if (invalues.IsEmpty())	return 	FVector2D();
	FVector2D tempresult{ -MAX_FLT,MAX_FLT };
	for (float value : invalues) {
		if (value > tempresult.X)tempresult.X = value;
		if (value < tempresult.Y)tempresult.Y = value;
	}
	return tempresult;
}
