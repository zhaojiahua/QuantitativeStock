// Fill out your copyright notice in the Description page of Project Settings.


#include "QuantitativeTradingCanves.h"
#include "QTCurveVectorActor.h"
#include "QuantitativeTradingWidget.h"
#include "KLineFloatWindWidget.h"
#include "CompanyNameIndexWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Curves/CurveVector.h"
#include "Rendering/DrawElementTypes.h"

void UQuantitativeTradingCanves::ReDrawSpecifyIndicator(FString inSpecifyName, FVector3f incycles){
	int tempint[3];
	tempint[0] = incycles.X;
	tempint[1] = incycles.Y;
	tempint[2] = incycles.Z;
	ReCaculateSpecifyIndicator(inSpecifyName, tempint);
	RefreshVisibleRows();
	ReSampleSpecifyIndicator(inSpecifyName);
}

void UQuantitativeTradingCanves::SetSpecifyIndicatorShow(FString inSpecifyName, bool showornot){
	if (inSpecifyName == "RSI0") { rsi0show = showornot; return; }
	if (inSpecifyName == "RSI1") { rsi1show = showornot; return; }
	if (inSpecifyName == "RSI2") { rsi2show = showornot; return; }
	if (inSpecifyName == "BIAS0") { bias0show = showornot; return; }
	if (inSpecifyName == "BIAS1") { bias1show = showornot; return; }
	if (inSpecifyName == "BIAS2") { bias2show = showornot; return; }
}

void UQuantitativeTradingCanves::SetMAVisible(UQTTreeViewItemObj* inItemObj){
	if (inItemObj->itemName == "SMA5") sma5 = inItemObj->isChecked;
	else if (inItemObj->itemName == "SMA10") sma10 = inItemObj->isChecked;
	else if (inItemObj->itemName == "SMA20") sma20 = inItemObj->isChecked;
	else if (inItemObj->itemName == "SMA60") sma60 = inItemObj->isChecked;
	else if (inItemObj->itemName == "SMA240") sma240 = inItemObj->isChecked;
	else if (inItemObj->itemName == "EMA5") ema5 = inItemObj->isChecked;
	else if (inItemObj->itemName == "EMA10") ema10 = inItemObj->isChecked;
	else if (inItemObj->itemName == "EMA20") ema20 = inItemObj->isChecked;
	else if (inItemObj->itemName == "EMA60") ema60 = inItemObj->isChecked;
	else if (inItemObj->itemName == "EMA240") ema240 = inItemObj->isChecked;
	else if (inItemObj->itemName == "Boll") boll = inItemObj->isChecked;
	else if (inItemObj->itemName == "KLine") kline = inItemObj->isChecked;
}

void UQuantitativeTradingCanves::InitialCanvas(){
	sma5 = false;
	sma10 = false;
	sma20 = false;
	sma60 = false;
	sma240 = false;
	ema5 = false;
	ema10 = false;
	ema20 = false;
	ema60 = false;
	ema240 = false;
}

void UQuantitativeTradingCanves::GetIntervalRowByStringItem(int inItemIndex){
	startDate = currentDate;
	//拆解当前日期
	int dateYear = currentDate / 10000;
	int dateMouthAndDay = currentDate % 10000;
	int dateMouth = dateMouthAndDay / 100;
	int dateDay = dateMouthAndDay % 100;
	int startYear = dateYear;
	int startMouth = dateMouth;
	if (inItemIndex > 9999)startDate -= inItemIndex;
	else {
		if (dateMouth > inItemIndex)startMouth -= inItemIndex;
		else { 
			startMouth = dateMouth + 12 - inItemIndex;
			startYear -= 1;
		}
		//拼接起始日期
		startDate = startYear * 10000 + startMouth * 100 + dateDay;
	}
	//截取数据到visibleRows
	GetIntervalRow(allStockIndexRows, visibleRows, startDate, currentDate);
	//从visibleRows重新采样数据
	sampledPoints = SampleDataFromDataTable();
	//重置可见数据参数
	leftOutCounts = 0;
	rightOutCounts = 0;
	visibleCounts = visibleRows.Num();
}

void UQuantitativeTradingCanves::OnIndicatorItemChanged(FName inIndicatorName){
	indicatorName = inIndicatorName;
	BroadcastIndicatorValueRangeByIndicatorName(indicatorName);
}

void UQuantitativeTradingCanves::UpdateLatestDayLine(FQTStockRealTimeData inRealTimeData){
	if (companyNameIndexWidget) {//首先更新companyNameIndexWidget里存储的最新数据和json文件
		companyNameIndexWidget->UpdateLatestDayLine(inRealTimeData);
	}
	//然后更新K线图的最新数据
	//重新计算最新数据的K线数据和各种指标数据
	ReCaculateAndStoreLatestDayKLine(inRealTimeData);
	//重新采样最新数据的K线数据和各种指标数据
	RefreshVisibleRows();
	SampleDataFromDataTable();
}

void UQuantitativeTradingCanves::LoadCycleSettingsFromJson_BP(const FString& inIndicatorName, int& out1, int& out2, int& out3){
	int tempint[3];
	LoadCycleSettingsFromJson(inIndicatorName, tempint);
	out1 = tempint[0];
	out2 = tempint[1];
	out3 = tempint[2];
}

TArray<FVector2f> UQuantitativeTradingCanves::SampleDataFromCurve(UCurveVector* inVectorCrv, const FGeometry& AllottedGeometry, int dimension)const
{
	TArray<FVector2f> tempArray;
		if (inVectorCrv == nullptr){
			//UE_LOG(LogTemp, Error, TEXT("----->> inVectorCrv is null"));
			return tempArray;
	}
	FRichCurve dCrv0 = inVectorCrv->FloatCurves[0];
	FRichCurve dCrv1 = inVectorCrv->FloatCurves[1];
	FRichCurve dCrv2 = inVectorCrv->FloatCurves[2];
	FRichCurve dCrv = inVectorCrv->FloatCurves[FMath::Clamp(dimension, 0, 2)];
	if (dCrv.GetNumKeys() < 2) {
		//UE_LOG(LogTemp, Error, TEXT("----- there is none keys on dimension -----"));
		return tempArray;
	}
	FDeprecateSlateVector2D geoSize = AllottedGeometry.GetLocalSize();
	float minTime, maxTime, minValue, maxValue;
	dCrv.GetTimeRange(minTime, maxTime);
	dCrv.GetValueRange(minValue, maxValue);
	float timeLength = maxTime - minTime;
	float valueLength = maxValue - minValue;

	float minTime0, maxTime0, minValue0, maxValue0;
	float minTime1, maxTime1, minValue1, maxValue1;
	float minTime2, maxTime2, minValue2, maxValue2;
	dCrv0.GetTimeRange(minTime0, maxTime0);
	dCrv0.GetValueRange(minValue0, maxValue0);
	dCrv1.GetTimeRange(minTime1, maxTime1);
	dCrv1.GetValueRange(minValue1, maxValue1);
	dCrv2.GetTimeRange(minTime2, maxTime2);
	dCrv2.GetValueRange(minValue2, maxValue2);
	float localminTime = FMath::Min3(minTime0, minTime1, minTime2);
	float localminValue = FMath::Min3(minValue0, minValue1, minValue2);
	float localmaxTime = FMath::Max3(maxTime0, maxTime1, maxTime2);
	float localmaxValue = FMath::Max3(maxValue0, maxValue1, maxValue2);
	float xScale = timeLength / (localmaxTime - localminTime);
	float yScale = valueLength / (localmaxValue - localminValue);

	float timeStep = timeLength / (samplingCounts - 1);
	for (float i = minTime; i <= maxTime; i += timeStep) {
		FVector2f temp2d = FVector2f(i / timeLength, 1.f - dCrv.Eval(i) / valueLength);//把Y值翻转后的曲线缩放至1X1的方格里(缩到了左上角)
		temp2d = FVector2f(temp2d.X * geoSize.X, temp2d.Y * geoSize.Y);//按比例缩放至画布大小(这里缩放的原点都是左上角)
		temp2d=FVector2f(temp2d.X * xScale, temp2d.Y * yScale + geoSize.Y * (1 - yScale));//按照缩放比例缩小xy两个维度,x轴坐标原点在左边不用动,y轴缩放原点在顶部,缩放之后会离开底部,所以我们要把y轴的值落回到底部
		tempArray.Add(FVector2f(temp2d.X, temp2d.Y * 0.5f));//最后Y轴统一向上缩小0.5,这样Y轴从底部正好缩至画布中间
	}
	return tempArray;
}

void UQuantitativeTradingCanves::NativePreConstruct(){
	Super::NativePreConstruct();
}

void UQuantitativeTradingCanves::NativeTick(const FGeometry& MyGeometry, float InDeltaTime){
	Super::NativeTick(MyGeometry, InDeltaTime);
	//更新K线浮动窗口位置
	if (KLineFlotWindWidgets.Num() == KLineUnitPoints1.Num()) {
		FDeprecateSlateVector2D localSize = MyGeometry.GetLocalSize();
		int endIndex = allCounts - rightOutCounts;
		float leftMove = static_cast<float>(leftOutCounts) / static_cast<float>(allCounts);
		float finalSlcalX = localSize.X * (static_cast<float>(allCounts) / static_cast<float>(visibleCounts));
		for (int i = leftOutCounts; i < endIndex; ++i) {
			float xpos = (sampledPoints[i].X - leftMove) * finalSlcalX;
			float ypos1 = KLineUnitPoints1[i].X * localSize.Y;
			float ypos2 = KLineUnitPoints1[i].Y * localSize.Y;
			KLineFlotWindWidgets[i]->SetRenderTransform(FWidgetTransform(FVector2D(xpos - 6.0f, 0.0f), FVector2D(1.0f, 1.0f), FVector2D(0.0f, 0.0f), 0.0f));
			KLineFlotWindWidgets[i]->SetVisibility(ESlateVisibility::Visible);
			Cast<UKLineFloatWindWidget>(KLineFlotWindWidgets[i])->lineXScale = (110.0f / static_cast<float>(visibleCounts));
			Cast<UKLineFloatWindWidget>(KLineFlotWindWidgets[i])->lineYScale = ypos1 - ypos2;
			Cast<UKLineFloatWindWidget>(KLineFlotWindWidgets[i])->lineYPosition = ypos1;
		}
		for (int i = 0; i < leftOutCounts; ++i) KLineFlotWindWidgets[i]->SetVisibility(ESlateVisibility::Hidden);
		for (int i = endIndex; i < allCounts; ++i) KLineFlotWindWidgets[i]->SetVisibility(ESlateVisibility::Hidden);
	}
}

FReply UQuantitativeTradingCanves::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent){
	FDeprecateSlateVector2D localPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	FDeprecateSlateVector2D localSize = InGeometry.GetLocalSize();
	float mouseXPos = localPos.X / localSize.X;
	int rightoutCount = 0;
	if (mouseXPos < 0.125) rightoutCount = 4;
	else if(mouseXPos < 0.375) rightoutCount = 3;
	else if(mouseXPos < 0.625) rightoutCount = 2;
	else if (mouseXPos < 0.875) rightoutCount = 1;

	int leftoutCount = 4 - rightoutCount;

	int maxCounts = 0.9f * allCounts;
	if (InMouseEvent.GetWheelDelta() > 0) {
		if ((rightOutCounts + leftOutCounts) < maxCounts) {
			rightOutCounts += rightoutCount * InMouseEvent.GetWheelDelta();
			rightOutCounts = FMath::Min(rightOutCounts, maxCounts);
			leftOutCounts += leftoutCount * InMouseEvent.GetWheelDelta();
			leftOutCounts = FMath::Min(leftOutCounts, maxCounts);
		}
	}
	else {
		rightOutCounts += rightoutCount * InMouseEvent.GetWheelDelta();
		rightOutCounts = FMath::Max(rightOutCounts, 0);
		leftOutCounts += leftoutCount * InMouseEvent.GetWheelDelta();
		leftOutCounts = FMath::Max(leftOutCounts, 0);
	}

	visibleCounts = allCounts - leftOutCounts - rightOutCounts;
	return FReply::Handled();
}

FReply UQuantitativeTradingCanves::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent){
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton) { 
		preMousePos = InMouseEvent.GetScreenSpacePosition();
		startMove = true; 
		SetCursor(EMouseCursor::GrabHand);
	}
	if (quantitativeTradingWidget) {
		quantitativeTradingWidget->ClearScreen();
	}
	return FReply::Handled();
}

FReply UQuantitativeTradingCanves::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent){
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton) {
		startMove = false;
		SetCursor(EMouseCursor::Default);
	}
	return FReply::Handled();
}

FReply UQuantitativeTradingCanves::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent){
	if (startMove) {
		float deltaMoveX = (InMouseEvent.GetScreenSpacePosition() - preMousePos).X;
		visibleCounts = allCounts - leftOutCounts - rightOutCounts;
		int moveCount = 0.002f * deltaMoveX * visibleCounts;
		if (moveCount > 0) moveCount = FMath::Min(leftOutCounts, moveCount);
		else moveCount = -FMath::Min(rightOutCounts, -moveCount);
		leftOutCounts -= moveCount;
		leftOutCounts = FMath::Clamp(leftOutCounts, 0, 0.9f * allCounts);
		rightOutCounts += moveCount;
		rightOutCounts = FMath::Clamp(rightOutCounts, 0, 0.9f * allCounts);
		preMousePos = InMouseEvent.GetScreenSpacePosition();
	}
	return FReply::Handled();
}

int32 UQuantitativeTradingCanves::NativePaint(const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const {
	FPaintContext paintContext = FPaintContext(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	//在中间画一条虚线
	FVector2f startPosition = FVector2f(0.0f, AllottedGeometry.GetLocalSize().Y * 0.6f);
	FVector2f endPosition = FVector2f(AllottedGeometry.GetLocalSize().X, AllottedGeometry.GetLocalSize().Y * 0.6f);
	//FSlateDrawElement::MakeDashedLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), { startPosition ,endPosition}, ESlateDrawEffect::None, FLinearColor(0.9f, 0.95f, 0.0f));
	DrawDottedLine(paintContext, startPosition, endPosition, FLinearColor(0.9f, 0.95f, 0.0f), 0.5f);
	
	//绘制曲线上的点
	{
		//TArray<FVector2f> crvPointsX = SampleDataFromCurve(selfVectorCrv, AllottedGeometry, 0);
		//TArray<FVector2f> crvPointsY = SampleDataFromCurve(selfVectorCrv, AllottedGeometry, 1);
		//TArray<FVector2f> crvPointsZ = SampleDataFromCurve(selfVectorCrv, AllottedGeometry, 2);
		//FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), crvPointsX, ESlateDrawEffect::None, FLinearColor(1.0f, 0.6f, 0.6f), true, 2.0f);
		//FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), crvPointsY, ESlateDrawEffect::None, FLinearColor(0.6f, 1.0f, 0.6f), true, 2.0f);
		//FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), crvPointsZ, ESlateDrawEffect::None, FLinearColor(0.6f, 0.6f, 1.0f), true, 2.0f);
	}

	FDeprecateSlateVector2D localSize = AllottedGeometry.GetLocalSize();
	FDeprecateSlateVector2D absSize = AllottedGeometry.GetAbsoluteSize();
	int endIndex = allCounts - rightOutCounts;
	float leftMove = static_cast<float>(leftOutCounts) / static_cast<float>(allCounts);
	float finalSlcalX = localSize.X * (static_cast<float>(allCounts) / static_cast<float>(visibleCounts));
	//绘制收盘价曲线
	if (!kline && sampledPoints.Num() > 1) {
		TArray<FVector2f> closePoints;
		closePoints.Reserve(visibleCounts + 2);
		for (int i = leftOutCounts; i < endIndex; ++i) {
			closePoints.Add(FVector2f((sampledPoints[i].X - leftMove) * finalSlcalX, sampledPoints[i].Y * localSize.Y));
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), closePoints, ESlateDrawEffect::None, FLinearColor(0.6f, 0.8f, 0.2f), true, 2.0f);
	}
	//绘制布林线
	if (boll && BollUnitPoints.Num() > 1) {
		TArray<FVector2f> tempPointsMid;
		TArray<FVector2f> tempPointsUp;
		TArray<FVector2f> tempPointsLow;
		tempPointsMid.Reserve(BollUnitPoints.Num() + 2);
		tempPointsUp.Reserve(BollUnitPoints.Num() + 2);
		tempPointsLow.Reserve(BollUnitPoints.Num() + 2);
		for (int i = leftOutCounts; i < endIndex; ++i) {
			float tempix = (sampledPoints[i].X - leftMove) * finalSlcalX;
			tempPointsMid.Add(FVector2f(tempix, sma20UnitPoints[i] * localSize.Y));
			tempPointsUp.Add(FVector2f(tempix, BollUnitPoints[i].X * localSize.Y));
			tempPointsLow.Add(FVector2f(tempix, BollUnitPoints[i].Y * localSize.Y));
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), tempPointsMid, ESlateDrawEffect::None, FLinearColor(0.9f, 0.9f, 0.95f), true, 2.0f);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), tempPointsUp, ESlateDrawEffect::None, FLinearColor(0.8f, 0.8f, 0.95f), true, 2.0f);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), tempPointsLow, ESlateDrawEffect::None, FLinearColor(0.8f, 0.8f, 0.95f), true, 2.0f);
	}
	//绘制均线
{
	if (sma5 && sma5UnitPoints.Num() > 1) {
		TArray<FVector2f> tempPoints;
		tempPoints.Reserve(sma5UnitPoints.Num() + 2);
		for (int i = leftOutCounts; i < endIndex; ++i) tempPoints.Add(FVector2f((sampledPoints[i].X - leftMove) * finalSlcalX, sma5UnitPoints[i] * localSize.Y));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), tempPoints, ESlateDrawEffect::None, FLinearColor(0.8f, 0.1f, 0.1f), true, 2.0f);
	}
	if (sma10 && sma10UnitPoints.Num() > 1) {
		TArray<FVector2f> tempPoints;
		tempPoints.Reserve(sma10UnitPoints.Num() + 2);
		for (int i = leftOutCounts; i < endIndex; ++i) tempPoints.Add(FVector2f((sampledPoints[i].X - leftMove) * finalSlcalX, sma10UnitPoints[i] * localSize.Y));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), tempPoints, ESlateDrawEffect::None, FLinearColor(0.1f, 0.1f, 0.8f), true, 2.0f);
	}
	if (sma20 && sma20UnitPoints.Num() > 1) {
		TArray<FVector2f> tempPoints;
		tempPoints.Reserve(sma20UnitPoints.Num() + 2);
		for (int i = leftOutCounts; i < endIndex; ++i) tempPoints.Add(FVector2f((sampledPoints[i].X - leftMove) * finalSlcalX, sma20UnitPoints[i] * localSize.Y));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), tempPoints, ESlateDrawEffect::None, FLinearColor(0.8f, 0.8f, 0.1f), true, 2.0f);
	}
	if (sma60 && sma60UnitPoints.Num() > 1) {
		TArray<FVector2f> tempPoints;
		tempPoints.Reserve(sma60UnitPoints.Num() + 2);
		for (int i = leftOutCounts; i < endIndex; ++i) tempPoints.Add(FVector2f((sampledPoints[i].X - leftMove) * finalSlcalX, sma60UnitPoints[i] * localSize.Y));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), tempPoints, ESlateDrawEffect::None, FLinearColor(0.1f, 0.8f, 0.8f), true, 2.0f);
	}
	if (sma240 && sma240UnitPoints.Num() > 1) {
		TArray<FVector2f> tempPoints;
		tempPoints.Reserve(sma240UnitPoints.Num() + 2);
		for (int i = leftOutCounts; i < endIndex; ++i) tempPoints.Add(FVector2f((sampledPoints[i].X - leftMove) * finalSlcalX, sma240UnitPoints[i] * localSize.Y));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), tempPoints, ESlateDrawEffect::None, FLinearColor(0.8f, 0.1f, 0.8f), true, 2.0f);
	}
	if (ema5 && ema5UnitPoints.Num() > 1) {
		TArray<FVector2f> tempPoints;
		tempPoints.Reserve(ema5UnitPoints.Num() + 2);
		for (int i = leftOutCounts; i < endIndex; ++i) tempPoints.Add(FVector2f((sampledPoints[i].X - leftMove) * finalSlcalX, ema5UnitPoints[i] * localSize.Y));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), tempPoints, ESlateDrawEffect::None, FLinearColor(0.5f, 0.2f, 0.2f), true, 2.0f);
	}
	if (ema10 && ema10UnitPoints.Num() > 1) {
		TArray<FVector2f> tempPoints;
		tempPoints.Reserve(ema10UnitPoints.Num() + 2);
		for (int i = leftOutCounts; i < endIndex; ++i) tempPoints.Add(FVector2f((sampledPoints[i].X - leftMove) * finalSlcalX, ema10UnitPoints[i] * localSize.Y));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), tempPoints, ESlateDrawEffect::None, FLinearColor(0.2f, 0.5f, 0.2f), true, 2.0f);
	}
	if (ema20 && ema20UnitPoints.Num() > 1) {
		TArray<FVector2f> tempPoints;
		tempPoints.Reserve(ema20UnitPoints.Num() + 2);
		for (int i = leftOutCounts; i < endIndex; ++i) tempPoints.Add(FVector2f((sampledPoints[i].X - leftMove) * finalSlcalX, ema20UnitPoints[i] * localSize.Y));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), tempPoints, ESlateDrawEffect::None, FLinearColor(0.2f, 0.2f, 0.5f), true, 2.0f);
	}
	if (ema60 && ema60UnitPoints.Num() > 1) {
		TArray<FVector2f> tempPoints;
		tempPoints.Reserve(ema60UnitPoints.Num() + 2);
		for (int i = leftOutCounts; i < endIndex; ++i) tempPoints.Add(FVector2f((sampledPoints[i].X - leftMove) * finalSlcalX, ema60UnitPoints[i] * localSize.Y));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), tempPoints, ESlateDrawEffect::None, FLinearColor(0.5f, 0.5f, 0.2f), true, 2.0f);
	}
	if (ema240 && ema240UnitPoints.Num() > 1) {
		TArray<FVector2f> tempPoints;
		tempPoints.Reserve(ema240UnitPoints.Num() + 2);
		for (int i = leftOutCounts; i < endIndex; ++i) tempPoints.Add(FVector2f((sampledPoints[i].X - leftMove) * finalSlcalX, ema240UnitPoints[i] * localSize.Y));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), tempPoints, ESlateDrawEffect::None, FLinearColor(0.2f, 0.5f, 0.5f), true, 2.0f);
	}
}
	
//绘制K线图
float lineWidth = 0.8f * absSize.X / static_cast<float>(visibleCounts);
{
	if (kline && KLineUnitPoints1.Num() > 1) {
		//绘制K线的实体部分
		for (int i = leftOutCounts; i < endIndex; ++i) {
			TArray<FVector2f> tempPoints;
			tempPoints.Reserve(2);
			float xpos = (sampledPoints[i].X - leftMove) * finalSlcalX;
			tempPoints.Add(FVector2f(xpos, KLineUnitPoints1[i].X * localSize.Y));
			tempPoints.Add(FVector2f(xpos, KLineUnitPoints1[i].Y * localSize.Y));
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), tempPoints, ESlateDrawEffect::None, KLineUnitColors1[i], true, lineWidth);
		}
		//绘制K线的影线部分
		for (int i = leftOutCounts; i < endIndex; ++i) {
			TArray<FVector2f> tempPoints;
			tempPoints.Reserve(2);
			float xpos = (sampledPoints[i].X - leftMove) * finalSlcalX;
			tempPoints.Add(FVector2f(xpos, KLineUnitPoints2[i].X * localSize.Y));
			tempPoints.Add(FVector2f(xpos, KLineUnitPoints2[i].Y * localSize.Y));
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), tempPoints, ESlateDrawEffect::None, KLineUnitColors1[i], true, 1.0f);
		}
	}
}

//绘制各种指标图形
{
	//画边界线
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), { FVector2f(0.0f,808.0f),FVector2f(1920.0f,808.0f) }, ESlateDrawEffect::None, FLinearColor(0.5f, 0.5f, 0.5f), true, 1.0f);
	//绘制0轴
	DrawDottedLine(paintContext, FVector2f(0.0f, 909.0f), FVector2f(1920.0f, 909.0f), FLinearColor(0.4f, 0.4f, 0.4f), 0.2f);

	if (indicatorName == "Volume") {//绘制成交量柱形图
		for (int i = leftOutCounts; i < endIndex; ++i) {
			TArray<FVector2f> tempPoints;
			tempPoints.Reserve(2);
			float xpos = (sampledPoints[i].X - leftMove) * finalSlcalX;
			tempPoints.Add(FVector2f(xpos, volumeUnitPoints[i] * localSize.Y));
			tempPoints.Add(FVector2f(xpos, 1010.0f));
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), tempPoints, ESlateDrawEffect::None, KLineUnitColors1[i], true, lineWidth);
		}
	}
	else if (indicatorName == "MACD") {
		TArray<FVector2f> difArray, deaArray;
		for (int i = leftOutCounts; i < endIndex; ++i) {
			TArray<FVector2f> tempPoints;
			tempPoints.Reserve(2);
			float xpos = (sampledPoints[i].X - leftMove) * finalSlcalX;
			tempPoints.Add(FVector2f(xpos, MACDUnitPoints[i].Z * localSize.Y));
			tempPoints.Add(FVector2f(xpos, 909.0f));
			difArray.Add(FVector2f(xpos, MACDUnitPoints[i].X* localSize.Y));
			deaArray.Add(FVector2f(xpos, MACDUnitPoints[i].Y* localSize.Y));
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), tempPoints, ESlateDrawEffect::None, MACDUnitPoints[i].Z < 0.9 ? FLinearColor(0.9f, 0.1f, 0.1f) : FLinearColor(0.1f, 0.9f, 0.1f), true, lineWidth);
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), difArray, ESlateDrawEffect::None, FLinearColor(0.2f, 0.2f, 0.5f), true, 2.0f);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), deaArray, ESlateDrawEffect::None, FLinearColor(0.5f, 0.2f, 0.2f), true, 2.0f);
	}
	else if(indicatorName=="KDJ")	{
		TArray<FVector2f> KArray, DArray, JArray;
		for (int i = leftOutCounts; i < endIndex; ++i) {
			float xpos = (sampledPoints[i].X - leftMove) * finalSlcalX;
			KArray.Add(FVector2f(xpos, KDJUnitPoints[i].X * localSize.Y));
			DArray.Add(FVector2f(xpos, KDJUnitPoints[i].Y * localSize.Y));
			JArray.Add(FVector2f(xpos, KDJUnitPoints[i].Z * localSize.Y));
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), KArray, ESlateDrawEffect::None, FLinearColor(0.9f, 0.6f, 0.1f), true, 2.0f);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), DArray, ESlateDrawEffect::None, FLinearColor(0.1f, 0.9f, 0.6f), true, 2.0f);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), JArray, ESlateDrawEffect::None, FLinearColor(0.6f, 0.1f, 0.9f), true, 2.0f);
		//绘制超买超卖线
		DrawDottedLine(paintContext, FVector2f(0.0f, localSize.Y* kdjOutLines.X), FVector2f(localSize.X, localSize.Y* kdjOutLines.X), FLinearColor(0.9f, 0.2f, 0.2f), 0.2f);
		DrawDottedLine(paintContext, FVector2f(0.0f, localSize.Y* kdjOutLines.Y), FVector2f(localSize.X, localSize.Y* kdjOutLines.Y), FLinearColor(0.2f, 0.2f, 0.9f), 0.2f);
		DrawDottedLine(paintContext, FVector2f(0.0f, localSize.Y* kdjOutLines.Z), FVector2f(localSize.X, localSize.Y* kdjOutLines.Z), FLinearColor(0.2f, 0.2f, 0.9f), 0.2f);
		DrawDottedLine(paintContext, FVector2f(0.0f, localSize.Y* kdjOutLines.W), FVector2f(localSize.X, localSize.Y* kdjOutLines.W), FLinearColor(0.9f, 0.2f, 0.2f), 0.2f);
	}
	else if (indicatorName == "RSI") {
		TArray<FVector2f> rsiArray0, rsiArray1, rsiArray2;
		for (int i = leftOutCounts; i < endIndex; ++i) {
			float xpos = (sampledPoints[i].X - leftMove) * finalSlcalX;
			rsiArray0.Add(FVector2f(xpos, RSIUnitPoints0[i] * localSize.Y));
			rsiArray1.Add(FVector2f(xpos, RSIUnitPoints1[i] * localSize.Y));
			rsiArray2.Add(FVector2f(xpos, RSIUnitPoints2[i] * localSize.Y));
		}
		if (rsi0show)FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), rsiArray0, ESlateDrawEffect::None, FLinearColor(0.9f, 0.1f, 0.9f), true, 2.0f);
		if (rsi1show)FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), rsiArray1, ESlateDrawEffect::None, FLinearColor(0.1f, 0.9f, 0.9f), true, 2.0f);
		if (rsi2show)FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), rsiArray2, ESlateDrawEffect::None, FLinearColor(0.9f, 0.9f, 0.1f), true, 2.0f);
		//绘制超买超卖线
		DrawDottedLine(paintContext, FVector2f(0.0f, localSize.Y* rsiOutLines.X), FVector2f(localSize.X, localSize.Y* rsiOutLines.X), FLinearColor(0.9f, 0.2f, 0.2f), 0.2f);
		DrawDottedLine(paintContext, FVector2f(0.0f, localSize.Y* rsiOutLines.Y), FVector2f(localSize.X, localSize.Y* rsiOutLines.Y), FLinearColor(0.2f, 0.2f, 0.9f), 0.2f);
	}
	else if (indicatorName == "WR") {
		TArray<FVector2f> wrArray1, wrArray2;
		for (int i = leftOutCounts; i < endIndex; ++i) {
			float xpos = (sampledPoints[i].X - leftMove) * finalSlcalX;
			wrArray1.Add(FVector2f(xpos, WRUnitPoints1[i] * localSize.Y));
			wrArray2.Add(FVector2f(xpos, WRUnitPoints2[i] * localSize.Y));
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), wrArray1, ESlateDrawEffect::None, FLinearColor(0.4f, 0.7f, 1.0f), true, 2.0f);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), wrArray2, ESlateDrawEffect::None, FLinearColor(1.0f, 0.7f, 0.4f), true, 2.0f);
		//绘制超买超卖线
		DrawDottedLine(paintContext, FVector2f(0.0f, localSize.Y* wrOutLines.X), FVector2f(localSize.X, localSize.Y* wrOutLines.X), FLinearColor(0.9f, 0.2f, 0.2f), 0.2f);
		DrawDottedLine(paintContext, FVector2f(0.0f, localSize.Y* wrOutLines.Y), FVector2f(localSize.X, localSize.Y* wrOutLines.Y), FLinearColor(0.2f, 0.2f, 0.9f), 0.2f);
	}
	else if (indicatorName == "DMI") {
		TArray<FVector2f> dmiArray0, dmiArray1, dmiArray2, dmiArray3;
		for (int i = leftOutCounts; i < endIndex; ++i) {
			float xpos = (sampledPoints[i].X - leftMove) * finalSlcalX;
			dmiArray0.Add(FVector2f(xpos, DMIUnitPoints[i].X * localSize.Y));
			dmiArray1.Add(FVector2f(xpos, DMIUnitPoints[i].Y * localSize.Y));
			dmiArray2.Add(FVector2f(xpos, DMIUnitPoints[i].Z * localSize.Y));
			dmiArray3.Add(FVector2f(xpos, DMIUnitPoints[i].W* localSize.Y));
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), dmiArray0, ESlateDrawEffect::None, FLinearColor(0.9f, 0.1f, 0.1f), true, 2.0f);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), dmiArray1, ESlateDrawEffect::None, FLinearColor(0.1f, 0.9f, 0.1f), true, 2.0f);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), dmiArray2, ESlateDrawEffect::None, FLinearColor(0.1f, 0.1f, 0.9f), true, 2.0f);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), dmiArray3, ESlateDrawEffect::None, FLinearColor(0.9f, 0.9f, 0.1f), true, 2.0f);
		//绘制判定线
		DrawDottedLine(paintContext, FVector2f(0.0f, localSize.Y* dmiOutLines.X), FVector2f(localSize.X, localSize.Y* dmiOutLines.X), FLinearColor(0.9f, 0.2f, 0.2f), 0.2f);
		DrawDottedLine(paintContext, FVector2f(0.0f, localSize.Y* dmiOutLines.Y), FVector2f(localSize.X, localSize.Y* dmiOutLines.Y), FLinearColor(0.2f, 0.2f, 0.9f), 0.2f);
	}
	else if (indicatorName == "CCI") {
		TArray<FVector2f> cciArray;
		for (int i = leftOutCounts; i < endIndex; ++i) {
			cciArray.Add(FVector2f((sampledPoints[i].X - leftMove)* finalSlcalX, CCIUnitPoints[i] * localSize.Y));
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), cciArray, ESlateDrawEffect::None, FLinearColor(0.2f, 0.3f, 1.0f), true, 2.0f);
		//绘制判定线
		DrawDottedLine(paintContext, FVector2f(0.0f, localSize.Y * cciOutLines.X), FVector2f(localSize.X, localSize.Y * cciOutLines.X), FLinearColor(0.9f, 0.2f, 0.2f), 0.2f);
		DrawDottedLine(paintContext, FVector2f(0.0f, localSize.Y * cciOutLines.Y), FVector2f(localSize.X, localSize.Y * cciOutLines.Y), FLinearColor(0.2f, 0.9f, 0.2f), 0.2f);
	}
	else if (indicatorName == "BIAS") {
		TArray<FVector2f> biasArray0, biasArray1, biasArray2;
		for (int i = leftOutCounts; i < endIndex; ++i) {
			float xpos = (sampledPoints[i].X - leftMove) * finalSlcalX;
			biasArray0.Add(FVector2f(xpos, BIASUnitPoints0[i] * localSize.Y));
			biasArray1.Add(FVector2f(xpos, BIASUnitPoints1[i] * localSize.Y));
			biasArray2.Add(FVector2f(xpos, BIASUnitPoints2[i] * localSize.Y));
		}
		if (bias0show)FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), biasArray0, ESlateDrawEffect::None, FLinearColor(0.9f, 0.1f, 0.9f), true, 2.0f);
		if (bias1show)FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), biasArray1, ESlateDrawEffect::None, FLinearColor(0.1f, 0.9f, 0.9f), true, 2.0f);
		if (bias2show)FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), biasArray2, ESlateDrawEffect::None, FLinearColor(0.9f, 0.9f, 0.1f), true, 2.0f);
		//绘制判定线
		DrawDottedLine(paintContext, FVector2f(0.0f, localSize.Y * biasOutLines.X), FVector2f(localSize.X, localSize.Y * biasOutLines.X), FLinearColor(0.9f, 0.2f, 0.2f), 0.2f);
		DrawDottedLine(paintContext, FVector2f(0.0f, localSize.Y * biasOutLines.Y), FVector2f(localSize.X, localSize.Y * biasOutLines.Y), FLinearColor(0.2f, 0.2f, 0.9f), 0.2f);
	}
}
	return LayerId;
}

void UQuantitativeTradingCanves::DrawDottedLine(const FPaintContext& paintContext, FVector2f startPosition, FVector2f endPosition, FLinearColor lineColor, float space) const
{
	int dottedCounts = 100 / space;
	FVector2f deltaVector = (endPosition - startPosition) / dottedCounts;
	for (int i = 0; i < dottedCounts; i += 2) {
		FVector2f localStartPosition = startPosition + deltaVector * i;
		FVector2f localEndPosition = startPosition + deltaVector * (i + 1.3f);
		FSlateDrawElement::MakeLines(paintContext.OutDrawElements, paintContext.LayerId, paintContext.AllottedGeometry.ToPaintGeometry(), { localStartPosition ,localEndPosition }, ESlateDrawEffect::None, lineColor);
	}
}

void UQuantitativeTradingCanves::GetIntervalRow(const TArray<TSharedPtr<FQTStockIndex>>& inallRows, TArray<TSharedPtr<FQTStockIndex>>& outVisibleRows,int cutStart, int cutEnd){
	outVisibleRows.Empty();
	for (auto item : inallRows) {
		if (item->Date >= cutStart && item->Date <= cutEnd) outVisibleRows.Add(item);
	}
}

TArray<FVector2f>UQuantitativeTradingCanves::SampleDataFromDataTable() {
	allCounts = visibleRows.Num();
	TArray<FVector2f> tempArray;
	float minClose = MAX_FLT, maxClose = -MAX_FLT;
	minVolume = MAX_FLT, maxVolume = -MAX_FLT;
	minKDJ = MAX_FLT, maxKDJ = -MAX_FLT;
	minRSI = MAX_FLT, maxRSI = -MAX_FLT;
	minWR = 100.0f, maxWR = 0.0f;
	minDMI = 100.0f, maxDMI= 0.0f;
	minCCI = MAX_FLT, maxCCI= -MAX_FLT;
	minBIAS= MAX_FLT, maxBIAS = -MAX_FLT;
	absmaxMACD = 0.0f;
	for (auto item : visibleRows) {
		if (item->Close < minClose)minClose = item->Close;
		if (item->Close > maxClose)maxClose = item->Close;
		if (item->Volume < minVolume)minVolume = item->Volume;
		if (item->Volume > maxVolume)maxVolume = item->Volume;
		if (FMath::Abs(item->MACD) > absmaxMACD) { absmaxMACD = FMath::Abs(item->MACD); }
		if (FMath::Abs(item->DEA) > absmaxMACD) { absmaxMACD = FMath::Abs(item->DEA); }
		if (FMath::Abs(item->DIF) > absmaxMACD) { absmaxMACD = FMath::Abs(item->DIF); }
		if (item->KDJ_K > maxKDJ) maxKDJ = item->KDJ_K;
		if (item->KDJ_D > maxKDJ) maxKDJ = item->KDJ_D;
		if (item->KDJ_J > maxKDJ) maxKDJ = item->KDJ_J;
		if (item->KDJ_K < minKDJ) minKDJ = item->KDJ_K;
		if (item->KDJ_D < minKDJ) minKDJ = item->KDJ_D;
		if (item->KDJ_J < minKDJ) minKDJ = item->KDJ_J;
		if (item->RSI0 > maxRSI) maxRSI = item->RSI0;
		if (item->RSI0 < minRSI) minRSI = item->RSI0;
		if (item->RSI1 > maxRSI) maxRSI = item->RSI1;
		if (item->RSI1 < minRSI) minRSI = item->RSI1;
		if (item->RSI2 > maxRSI) maxRSI = item->RSI2;
		if (item->RSI2 < minRSI) minRSI = item->RSI2;
		if (item->WR1 > maxWR) maxWR = item->WR1;
		if (item->WR1 < minWR) minWR = item->WR1;
		if (item->WR2 > maxWR) maxWR = item->WR2;
		if (item->WR2 < minWR) minWR = item->WR2;
		if (item->PDI > maxDMI) maxDMI = item->PDI;
		if (item->PDI < minDMI) minDMI = item->PDI;
		if (item->NDI > maxDMI) maxDMI = item->NDI;
		if (item->NDI < minDMI) minDMI = item->NDI;
		if (item->ADX > maxDMI) maxDMI = item->ADX;
		if (item->ADX < minDMI) minDMI = item->ADX;
		if (item->ADXR > maxDMI) maxDMI = item->ADXR;
		if (item->ADXR < minDMI) minDMI = item->ADXR;
		if (item->CCI > maxCCI) maxCCI = item->CCI;
		if (item->CCI < minCCI) minCCI = item->CCI;
		if (item->BIAS0 > maxBIAS) maxBIAS = item->BIAS0;
		if (item->BIAS0 < minBIAS) minBIAS = item->BIAS0;
		if (item->BIAS1 > maxBIAS) maxBIAS = item->BIAS1;
		if (item->BIAS1 < minBIAS) minBIAS = item->BIAS1;
		if (item->BIAS2 > maxBIAS) maxBIAS = item->BIAS2;
		if (item->BIAS2 < minBIAS) minBIAS = item->BIAS2;
	}
	BroadcastIndicatorValueRangeByIndicatorName(indicatorName);
	float closeValueRange = maxClose - minClose;
	float volumeValueRange = maxVolume - minVolume;
	float KDJValueRange = maxKDJ - minKDJ;
	float RSIValueRange = maxRSI - minRSI;
	float WRValueRange = maxWR - minWR;
	float DMIValueRange = maxDMI - minDMI;
	float CCIValueRange = maxCCI - minCCI;
	float BIASValueRange = maxBIAS - minBIAS;
	kdjOutLines.X = ((1.f - (100.0f - minKDJ) / KDJValueRange) * 0.2f + 0.8f);
	kdjOutLines.Y = ((1.f - (80.0f - minKDJ) / KDJValueRange) * 0.2f + 0.8f);
	kdjOutLines.Z = ((1.f - (20.0f - minKDJ) / KDJValueRange) * 0.2f + 0.8f);
	kdjOutLines.W = ((1.f - (0.0f - minKDJ) / KDJValueRange) * 0.2f + 0.8f);
	rsiOutLines.X = ((1.f - (70.0f - minRSI) / RSIValueRange) * 0.2f + 0.8f);
	rsiOutLines.Y = ((1.f - (30.0f - minRSI) / RSIValueRange) * 0.2f + 0.8f);
	wrOutLines.X = (((80.0f - minWR) / WRValueRange) * 0.2f + 0.8f);
	wrOutLines.Y = (((20.0f - minWR) / WRValueRange) * 0.2f + 0.8f);
	dmiOutLines.X = ((1.f - (50.0f - minDMI) / DMIValueRange) * 0.2f + 0.8f);
	dmiOutLines.Y = ((1.f - (20.0f - minDMI) / DMIValueRange) * 0.2f + 0.8f);
	cciOutLines.X = ((1.f - (100.0f - minCCI) / CCIValueRange) * 0.2f + 0.8f);
	cciOutLines.Y = ((1.f - (-100.0f - minCCI) / CCIValueRange) * 0.2f + 0.8f);
	biasOutLines.X = ((1.f - (5.0f - minBIAS) / BIASValueRange) * 0.2f + 0.8f);
	biasOutLines.Y = ((1.f - (-5.0f - minBIAS) / BIASValueRange) * 0.2f + 0.8f);
	float timeRange = visibleRows.Num() + 1;
	float timeStep = timeRange / (samplingCounts - 1);
	sma5UnitPoints.Empty();
	sma10UnitPoints.Empty();
	sma20UnitPoints.Empty();
	sma60UnitPoints.Empty();
	sma240UnitPoints.Empty();
	ema5UnitPoints.Empty();
	ema10UnitPoints.Empty();
	ema20UnitPoints.Empty();
	ema60UnitPoints.Empty();
	ema240UnitPoints.Empty();
	KLineUnitColors1.Empty();
	KLineUnitPoints1.Empty();
	KLineUnitPoints2.Empty();
	volumeUnitPoints.Empty();
	MACDUnitPoints.Empty();
	BollUnitPoints.Empty();
	KDJUnitPoints.Empty();
	RSIUnitPoints0.Empty();
	RSIUnitPoints1.Empty();
	RSIUnitPoints2.Empty();
	WRUnitPoints1.Empty();
	WRUnitPoints2.Empty();
	DMIUnitPoints.Empty();
	CCIUnitPoints.Empty();
	BIASUnitPoints0.Empty();
	BIASUnitPoints1.Empty();
	BIASUnitPoints2.Empty();
	for (auto& item : KLineFlotWindWidgets) item->RemoveFromParent();
	KLineFlotWindWidgets.Empty();
	sma5UnitPoints.Reserve(visibleRows.Num());
	sma10UnitPoints.Reserve(visibleRows.Num());
	sma20UnitPoints.Reserve(visibleRows.Num());
	sma60UnitPoints.Reserve(visibleRows.Num());
	sma240UnitPoints.Reserve(visibleRows.Num());
	ema5UnitPoints.Reserve(visibleRows.Num());
	ema10UnitPoints.Reserve(visibleRows.Num());
	ema20UnitPoints.Reserve(visibleRows.Num());
	ema60UnitPoints.Reserve(visibleRows.Num());
	ema240UnitPoints.Reserve(visibleRows.Num());
	KLineUnitColors1.Reserve(visibleRows.Num());
	KLineUnitPoints1.Reserve(visibleRows.Num());
	KLineUnitPoints2.Reserve(visibleRows.Num());
	volumeUnitPoints.Reserve(visibleRows.Num());
	KLineFlotWindWidgets.Reserve(visibleRows.Num());
	MACDUnitPoints.Reserve(visibleRows.Num());
	BollUnitPoints.Reserve(visibleRows.Num());
	KDJUnitPoints.Reserve(visibleRows.Num());
	RSIUnitPoints0.Reserve(visibleRows.Num());
	RSIUnitPoints1.Reserve(visibleRows.Num());
	RSIUnitPoints2.Reserve(visibleRows.Num());
	WRUnitPoints1.Reserve(visibleRows.Num());
	WRUnitPoints2.Reserve(visibleRows.Num());
	DMIUnitPoints.Reserve(visibleRows.Num());
	CCIUnitPoints.Reserve(visibleRows.Num());
	BIASUnitPoints0.Reserve(visibleRows.Num());
	BIASUnitPoints1.Reserve(visibleRows.Num());
	BIASUnitPoints2.Reserve(visibleRows.Num());
	for (int i = 0; i < visibleRows.Num(); ++i) {
		tempArray.Add(FVector2f((i + 1) / timeRange, (1.f - (visibleRows[i]->Close - minClose) / closeValueRange) * 0.6f));//把Y值翻转后的曲线缩放至1X1的方格里(缩到了左上角),最Y轴统一向上缩小0.5,这样Y轴从底部正好缩至画布中间
		volumeUnitPoints.Add((1.f - (visibleRows[i]->Volume - minVolume) / volumeValueRange) * 0.2f + 0.8f);
		float unitMACD = (1.f - (visibleRows[i]->MACD / absmaxMACD * 0.5f + 0.5f)) * 0.2f + 0.8f;
		float unitDIF = (1.f - (visibleRows[i]->DIF / absmaxMACD * 0.5f + 0.5f)) * 0.2f + 0.8f;
		float unitDEA = (1.f - (visibleRows[i]->DEA / absmaxMACD * 0.5f + 0.5f)) * 0.2f + 0.8f;
		MACDUnitPoints.Add(FVector3f(unitDIF, unitDEA, unitMACD));
		KDJUnitPoints.Add(FVector3f((1.f - (visibleRows[i]->KDJ_K - minKDJ) / KDJValueRange) * 0.2f + 0.8f,
			(1.f - (visibleRows[i]->KDJ_D - minKDJ) / KDJValueRange) * 0.2f + 0.8f,
			(1.f - (visibleRows[i]->KDJ_J - minKDJ) / KDJValueRange) * 0.2f + 0.8f));
		RSIUnitPoints0.Add((1.f - (visibleRows[i]->RSI0 - minRSI) / RSIValueRange) * 0.2f + 0.8f);
		RSIUnitPoints1.Add((1.f - (visibleRows[i]->RSI1 - minRSI) / RSIValueRange) * 0.2f + 0.8f);
		RSIUnitPoints2.Add((1.f - (visibleRows[i]->RSI2 - minRSI) / RSIValueRange) * 0.2f + 0.8f);
		WRUnitPoints1.Add(((visibleRows[i]->WR1 - minWR) / WRValueRange) * 0.2f + 0.8f);
		WRUnitPoints2.Add(((visibleRows[i]->WR2 - minWR) / WRValueRange) * 0.2f + 0.8f);
		DMIUnitPoints.Add(FVector4f((1.0f - (visibleRows[i]->PDI - minDMI) / DMIValueRange) * 0.2f + 0.8f,
			(1.0f - (visibleRows[i]->NDI - minDMI) / DMIValueRange) * 0.2f + 0.8f,
			(1.0f - (visibleRows[i]->ADX - minDMI) / DMIValueRange) * 0.2f + 0.8f,
			(1.0f - (visibleRows[i]->ADXR - minDMI) / DMIValueRange) * 0.2f + 0.8f));
		CCIUnitPoints.Add((1.f - (visibleRows[i]->CCI - minCCI) / CCIValueRange) * 0.2f + 0.8f);
		BIASUnitPoints0.Add((1.f - (visibleRows[i]->BIAS0 - minBIAS) / BIASValueRange) * 0.2f + 0.8f);
		BIASUnitPoints1.Add((1.f - (visibleRows[i]->BIAS1 - minBIAS) / BIASValueRange) * 0.2f + 0.8f);
		BIASUnitPoints2.Add((1.f - (visibleRows[i]->BIAS2 - minBIAS) / BIASValueRange) * 0.2f + 0.8f);
		sma5UnitPoints.Add((1.f - (visibleRows[i]->SMA5 - minClose) / closeValueRange) * 0.6f);
		sma10UnitPoints.Add((1.f - (visibleRows[i]->SMA10 - minClose) / closeValueRange) * 0.6f);
		sma20UnitPoints.Add((1.f - (visibleRows[i]->SMA20 - minClose) / closeValueRange) * 0.6f);
		sma60UnitPoints.Add((1.f - (visibleRows[i]->SMA60 - minClose) / closeValueRange) * 0.6f);
		sma240UnitPoints.Add((1.f - (visibleRows[i]->SMA240 - minClose) / closeValueRange) * 0.6f);
		ema5UnitPoints.Add((1.f - (visibleRows[i]->EMA5 - minClose) / closeValueRange) * 0.6f);
		ema10UnitPoints.Add((1.f - (visibleRows[i]->EMA10 - minClose) / closeValueRange) * 0.6f);
		ema20UnitPoints.Add((1.f - (visibleRows[i]->EMA20 - minClose) / closeValueRange) * 0.6f);
		ema60UnitPoints.Add((1.f - (visibleRows[i]->EMA60 - minClose) / closeValueRange) * 0.6f);
		ema240UnitPoints.Add((1.f - (visibleRows[i]->EMA240 - minClose) / closeValueRange) * 0.6f);
		float xupper = (1.f - (visibleRows[i]->BollUpper - minClose) / closeValueRange) * 0.6f;
		float ylower = (1.f - (visibleRows[i]->BollLower - minClose) / closeValueRange) * 0.6f;
		BollUnitPoints.Add(FVector2f(xupper, ylower));
		if (visibleRows[i]->Open > visibleRows[i]->Close) {
			KLineUnitPoints1.Add(FVector2f((1.f - (visibleRows[i]->Open - minClose) / closeValueRange) * 0.6f, (1.f - (visibleRows[i]->Close - minClose) / closeValueRange) * 0.6f));//存储开盘价和收盘价
			KLineUnitColors1.Add(FLinearColor(0.1f, 0.8f, 0.1f));//绿色代表股价下跌
		}
		else {
			KLineUnitPoints1.Add(FVector2f((1.f - (visibleRows[i]->Close - minClose) / closeValueRange) * 0.6f, (1.f - (visibleRows[i]->Open - minClose) / closeValueRange) * 0.6f));//存储收盘价和开盘价
			KLineUnitColors1.Add(FLinearColor(0.8f, 0.1f, 0.1f));//红色代表股价上涨
		}
		KLineUnitPoints2.Add(FVector2f((1.f - (visibleRows[i]->High - minClose) / closeValueRange) * 0.6f, (1.f - (visibleRows[i]->Low - minClose) / closeValueRange) * 0.6f));//存储最高价
		if (KLineFlotWindWidgetClass) {
			UUserWidget* KLineFlotWindWidgetInstance = CreateWidget<UUserWidget>(GetWorld(), KLineFlotWindWidgetClass);
			KLineFlotWindWidgetInstance->AddToViewport();
			overlayForFloatWind->AddChild(KLineFlotWindWidgetInstance);
			Cast<UKLineFloatWindWidget>(KLineFlotWindWidgetInstance)->stockInfoDatas = *visibleRows[i];
			KLineFlotWindWidgets.Add(KLineFlotWindWidgetInstance);
		}

	}
	return tempArray;
}

void UQuantitativeTradingCanves::CaculateAndStoreIndicators(TArray<TSharedPtr<FQTStockIndex>>& allRows) {
	//初始化第一个交易日的MA值
	float tempSum = 0;
	for (int i = 0; i < 240; ++i) {
		tempSum += allRows[i]->Close;
		if (i == 4) allRows[4]->SMA5SUM = tempSum;
		if (i == 9) allRows[9]->SMA10SUM = tempSum;
		if (i == 19) allRows[19]->SMA20SUM = tempSum;
		if (i == 59) allRows[59]->SMA60SUM = tempSum;
		if (i == 239) allRows[239]->SMA240SUM = tempSum;
	}
	allRows[4]->SMA5 = allRows[4]->SMA5SUM / 5.0f;
	allRows[9]->SMA10 = allRows[9]->SMA10SUM / 10.0f;
	allRows[19]->SMA20 = allRows[19]->SMA20SUM / 20.0f;
	float tempVarSum = 0.0f;
	for (int i = 0; i < 20; ++i) { tempVarSum += FMath::Square(allRows[i]->Close - allRows[19]->SMA20); }
	float SDev = FMath::Sqrt(tempVarSum / 20.0f);
	allRows[19]->BollUpper = allRows[19]->SMA20 + 2 * SDev;
	allRows[19]->BollLower = allRows[19]->SMA20 - 2 * SDev;
	allRows[59]->SMA60 = allRows[59]->SMA60SUM / 60.0f;
	allRows[239]->SMA60 = allRows[239]->SMA240SUM / 240.0f;
	allRows[0]->EMA5 = allRows[0]->Close;
	allRows[0]->EMA10 = allRows[0]->Close;
	allRows[0]->DIF1 = allRows[0]->Close;
	allRows[0]->EMA20 = allRows[0]->Close;
	allRows[0]->DIF2 = allRows[0]->Close;
	allRows[0]->EMA60 = allRows[0]->Close;
	allRows[0]->EMA240 = allRows[0]->Close;
	//KDJ初始值
	allRows[7]->KDJ_K = 50.0f;
	allRows[7]->KDJ_D = 50.0f;
	//设置15天的RSI,第11天的RSI,第25天的RSI的初始值
	{
		float upSum0 = 0.0f, downSum0 = 0.0f;
		float upSum1 = 0.0f, downSum1 = 0.0f;
		float upSum2 = 0.0f, downSum2 = 0.0f;
		for (int i = 1; i < 25; ++i) {
			float changeValue = allRows[i]->Close - allRows[i - 1]->Close;
			if (changeValue > 0) upSum2 += changeValue;
			else downSum2 -= changeValue;
			if (i == 10) { upSum1 = upSum2; downSum1 = downSum2; }
			if (i == 14) { upSum0 = upSum2; downSum0 = downSum2; }
		}
		allRows[14]->RSI0_AVGUp = upSum0 / 14.0f;
		allRows[14]->RSI0_AVGDown = downSum0 / 14.0f;
		allRows[10]->RSI1_AVGUp = upSum1 / 10.0f;
		allRows[10]->RSI1_AVGDown = downSum1 / 10.0f;
		allRows[24]->RSI2_AVGUp = upSum2 / 24.0f;
		allRows[24]->RSI2_AVGDown = downSum2 / 24.0f;
		float RS = allRows[14]->RSI0_AVGUp / allRows[14]->RSI0_AVGDown;
		allRows[14]->RSI0 = 100.0f - (100.0f / (1.0f + RS));
		RS = allRows[10]->RSI1_AVGUp / allRows[10]->RSI1_AVGDown;
		allRows[10]->RSI1 = 100.0f - (100.0f / (1.0f + RS));
		RS = allRows[24]->RSI2_AVGUp / allRows[24]->RSI2_AVGDown;
		allRows[24]->RSI2 = 100.0f - (100.0f / (1.0f + RS));
	}
	//设置DMI初始值
	int tempDMIcycles[3] = { 14,14,0 };
	SetFirstValues_DMI(allRows, tempDMIcycles);
	//设置CCI的初始值
	int tempCCIcycles[3] = { 20,0,0 };
	SetFirstValues_CCI(allRows, tempCCIcycles);
	//设置BIAS的初始值
	int tempBIAScycles0[3] = { 6,0,0 };
	SetFirstValues_BIAS(allRows, tempBIAScycles0, 0);
	int tempBIAScycles1[3] = { 12,0,0 };
	SetFirstValues_BIAS(allRows, tempBIAScycles1, 1);
	int tempBIAScycles2[3] = { 24,0,0 };
	SetFirstValues_BIAS(allRows, tempBIAScycles2, 2);
	//计算后续交易日的MA值
	float alpha5 = 2.0f / (5 + 1);
	float alpha9 = 2.0f / (9 + 1);
	float alpha10 = 2.0f / (10 + 1);
	float alpha12 = 2.0f / (12 + 1);
	float alpha20 = 2.0f / (20 + 1);
	float alpha26 = 2.0f / (26 + 1);
	float alpha60 = 2.0f / (60 + 1);
	float alpha240 = 2.0f / (240 + 1);
	for (int i = 1; i < allRows.Num(); i++) {
		if (i > 4) {//从第6个交易日开始计算SMA5 同时开始计算WR2
			allRows[i]->SMA5SUM = allRows[i - 1]->SMA5SUM - allRows[i - 5]->Close + allRows[i]->Close;
			allRows[i]->SMA5 = allRows[i]->SMA5SUM / 5.0f;
			//获取6个交易日内的最高价和最低价(含当天)
			float highest6 = 0.0f;
			float lowest6 = FLT_MAX;
			for (int j = 0; j < 6; ++j) {
				if (allRows[i - j]->Close > highest6) highest6 = allRows[i - j]->Close;
				if (allRows[i - j]->Close < lowest6) lowest6 = allRows[i - j]->Close;
			}
			//计算WR2
			if (highest6 == lowest6) allRows[i]->WR2 = 0.0f;
			else allRows[i]->WR2 = (highest6 - allRows[i]->Close) / (highest6 - lowest6) * 100.0f;
		}
		if (i > 5) {//从第7个交易日开始计算BIAS0
			CalculateAndStoreBIAS(allRows, i, tempBIAScycles0, 0);
		}
		if (i > 11) {//从第13个交易日开始计算BIAS1
			CalculateAndStoreBIAS(allRows, i, tempBIAScycles1, 1);
		}
		if (i > 23) {//从第25个交易日开始计算BIAS2
			CalculateAndStoreBIAS(allRows, i, tempBIAScycles2, 2);
		}
		if (i > 8) {//从第10个交易日开始计算WR1
			//获取10天内的最高价和最低价(含当天)
			float highest10 = 0.0f;
			float lowest10 = FLT_MAX;
			for (int j = 0; j < 10; ++j) {
				if (allRows[i - j]->Close > highest10) highest10 = allRows[i - j]->Close;
				if (allRows[i - j]->Close < lowest10) lowest10 = allRows[i - j]->Close;
			}
			//计算WR1
			if (highest10 == lowest10) allRows[i]->WR1 = 0.0f;
			else allRows[i]->WR1 = (highest10 - allRows[i]->Close) / (highest10 - lowest10) * 100.0f;
		}
		if (i > 7) {//从第9个交易日开始计算RSV
			//获取9天内的最高价和最低价
			float highest9 = 0.0f;
			float lowest9 = FLT_MAX;
			for (int j = 0; j < 9; ++j) {
				if (allRows[i - j]->Close > highest9) highest9 = allRows[i - j]->Close;
				if (allRows[i - j]->Close < lowest9) lowest9 = allRows[i - j]->Close;
			}
			//计算RSV
			if (highest9 == lowest9) allRows[i]->KDJ_RSV = 0.0f;
			else allRows[i]->KDJ_RSV = (allRows[i]->Close - lowest9) / (highest9 - lowest9) * 100.0f;
			//计算K值和D值和J值
			allRows[i]->KDJ_K = (2.0f / 3.0f) * allRows[i - 1]->KDJ_K + (1.0f / 3.0f) * allRows[i]->KDJ_RSV;
			allRows[i]->KDJ_D = (2.0f / 3.0f) * allRows[i - 1]->KDJ_D + (1.0f / 3.0f) * allRows[i]->KDJ_K;
			allRows[i]->KDJ_J = 3.0f * allRows[i]->KDJ_K - 2.0f * allRows[i]->KDJ_D;
		}
		if (i > 9) {
			allRows[i]->SMA10SUM = allRows[i - 1]->SMA10SUM - allRows[i - 10]->Close + allRows[i]->Close;
			allRows[i]->SMA10 = allRows[i]->SMA10SUM / 10.0f;
		}
		if (i > 10) {//从第12个交易日开始计算RSI1
			float changeValue = allRows[i]->Close - allRows[i - 1]->Close;
			if (changeValue > 0) {
				allRows[i]->RSI1_AVGUp = (allRows[i - 1]->RSI1_AVGUp * 9.0f + changeValue) / 10.0f;
				allRows[i]->RSI1_AVGDown = (allRows[i - 1]->RSI1_AVGDown * 9.0f) / 10.0f;
			}
			else {
				allRows[i]->RSI1_AVGDown = (allRows[i - 1]->RSI1_AVGDown * 9.0f - changeValue) / 10.0f;
				allRows[i]->RSI1_AVGUp = (allRows[i - 1]->RSI1_AVGUp * 9.0f) / 10.0f;
			}
			float RS = allRows[i]->RSI1_AVGUp / allRows[i]->RSI1_AVGDown;
			allRows[i]->RSI1 = 100.0f - (100.0f / (1.0f + RS));
		}
		//从第15个交易日开始计算DMI
		if (i > 13) CalculateAndStoreDMI(allRows, i, tempDMIcycles);
		if (i > 14) {//从第16个交易日开始计算RSI0
			float changeValue = allRows[i]->Close - allRows[i - 1]->Close;
			if (changeValue > 0) {
				allRows[i]->RSI0_AVGUp = (allRows[i - 1]->RSI0_AVGUp * 13.0f + changeValue) / 14.0f;
				allRows[i]->RSI0_AVGDown = (allRows[i - 1]->RSI0_AVGDown * 13.0f) / 14.0f;
			}
			else {
				allRows[i]->RSI0_AVGDown = (allRows[i - 1]->RSI0_AVGDown * 13.0f - changeValue) / 14.0f;
				allRows[i]->RSI0_AVGUp = (allRows[i - 1]->RSI0_AVGUp * 13.0f) / 14.0f;
			}
			float RS = allRows[i]->RSI0_AVGUp / allRows[i]->RSI0_AVGDown;
			allRows[i]->RSI0 = 100.0f - (100.0f / (1.0f + RS));
		}
		if (i > 24) {//从第26个交易日开始计算RSI2
			float changeValue = allRows[i]->Close - allRows[i - 1]->Close;
			if (changeValue > 0) {
				allRows[i]->RSI2_AVGUp = (allRows[i - 1]->RSI2_AVGUp * 23.0f + changeValue) / 24.0f;
				allRows[i]->RSI2_AVGDown = (allRows[i - 1]->RSI2_AVGDown * 23.0f) / 24.0f;
			}
			else {
				allRows[i]->RSI2_AVGDown = (allRows[i - 1]->RSI2_AVGDown * 23.0f - changeValue) / 24.0f;
				allRows[i]->RSI2_AVGUp = (allRows[i - 1]->RSI2_AVGUp * 23.0f) / 24.0f;
			}
			float RS = allRows[i]->RSI2_AVGUp / allRows[i]->RSI2_AVGDown;
			allRows[i]->RSI2 = 100.0f - (100.0f / (1.0f + RS));
		}
		if (i > 19) {//从第21个交易日开始计算SMA20和CCI
			allRows[i]->SMA20SUM = allRows[i - 1]->SMA20SUM - allRows[i - 20]->Close + allRows[i]->Close;
			allRows[i]->SMA20 = allRows[i]->SMA20SUM / 20.0f;
			float tempVarSum1 = 0.0f;
			for (int j = 0; j < 20; ++j) { tempVarSum1 += FMath::Square(allRows[i - j]->Close - allRows[i]->SMA20); }
			float SDev1 = FMath::Sqrt(tempVarSum1 / 20.0f);
			allRows[i]->BollUpper = allRows[i]->SMA20 + 2 * SDev1;
			allRows[i]->BollLower = allRows[i]->SMA20 - 2 * SDev1;
			//计算CCI
			CalculateAndStoreCCI(allRows, i, tempCCIcycles);
		}
		if (i > 59) {
			allRows[i]->SMA60SUM = allRows[i - 1]->SMA60SUM - allRows[i - 60]->Close + allRows[i]->Close;
			allRows[i]->SMA60 = allRows[i]->SMA60SUM / 60.0f;
		}
		if (i > 239) {
			allRows[i]->SMA240SUM = allRows[i - 1]->SMA240SUM - allRows[i - 240]->Close + allRows[i]->Close;
			allRows[i]->SMA240 = allRows[i]->SMA240SUM / 240.0f;
		}
		allRows[i]->EMA5 = allRows[i - 1]->EMA5 + alpha5 * (allRows[i]->Close - allRows[i - 1]->EMA5);
		allRows[i]->EMA10 = allRows[i - 1]->EMA10 + alpha10 * (allRows[i]->Close - allRows[i - 1]->EMA10);
		allRows[i]->DIF1 = allRows[i - 1]->DIF1 + alpha12 * (allRows[i]->Close - allRows[i - 1]->DIF1);
		allRows[i]->EMA20 = allRows[i - 1]->EMA20 + alpha20 * (allRows[i]->Close - allRows[i - 1]->EMA20);
		allRows[i]->DIF2 = allRows[i - 1]->DIF2 + alpha26 * (allRows[i]->Close - allRows[i - 1]->DIF2);
		allRows[i]->EMA60 = allRows[i - 1]->EMA60 + alpha60 * (allRows[i]->Close - allRows[i - 1]->EMA60);
		allRows[i]->EMA240 = allRows[i - 1]->EMA240 + alpha240 * (allRows[i]->Close - allRows[i - 1]->EMA240);
		allRows[i]->DIF = allRows[i]->DIF1 - allRows[i]->DIF2;
		allRows[i]->DEA = allRows[i - 1]->DEA + alpha9 * (allRows[i]->DIF - allRows[i - 1]->DEA);
		allRows[i]->MACD = (allRows[i]->DIF - allRows[i]->DEA) * 2.0f;
	}
}

bool UQuantitativeTradingCanves::SaveCycleSettingsToJson(const FString& inSpecifyName, const int cycleInfos[3]){
	FString paramFilePath = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/IndicatorParams/%s.json"), *indicatorName.ToString());
	TSharedPtr<FJsonObject> jsonObject = MakeShareable(new FJsonObject());
	int cycleinfosold[3];
	if (!LoadCycleSettingsFromJson(indicatorName.ToString(), cycleinfosold)) {
		UE_LOG(LogTemp, Warning, TEXT("Load indicator %s params failed!!"), *indicatorName.ToString());
		return false;
	}
	if (inSpecifyName == "DIF1" || inSpecifyName == "RSV"|| inSpecifyName == "RSI0" || inSpecifyName == "WR1" || inSpecifyName == "CCI" || inSpecifyName == "BIAS0" || inSpecifyName == "PDI" || inSpecifyName == "NDI" || inSpecifyName == "ADX") cycleinfosold[0] = cycleInfos[0];
	if (inSpecifyName == "DIF2" || inSpecifyName == "K" || inSpecifyName == "ADXR") cycleinfosold[1] = cycleInfos[1];
	if (inSpecifyName == "DEA" || inSpecifyName == "D") cycleinfosold[2] = cycleInfos[2];
	if(inSpecifyName == "RSI1"|| inSpecifyName == "WR2"|| inSpecifyName == "BIAS1") cycleinfosold[1] = cycleInfos[0];
	if (inSpecifyName == "RSI2"|| inSpecifyName == "BIAS2") cycleinfosold[2] = cycleInfos[0];
	jsonObject->SetNumberField("Cycle1", cycleinfosold[0]);
	jsonObject->SetNumberField("Cycle2", cycleinfosold[1]);
	jsonObject->SetNumberField("Cycle3", cycleinfosold[2]);
	FString outputString;
	TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&outputString);
	if (FJsonSerializer::Serialize(jsonObject.ToSharedRef(), writer)) {
		return FFileHelper::SaveStringToFile(outputString, *paramFilePath);
	}
	return false;
}

bool UQuantitativeTradingCanves::LoadCycleSettingsFromJson(const FString& inIndicatorName, int cycleInfos[3]) {
	FString paramFilePath = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/IndicatorParams/%s.json"), *inIndicatorName);
	FString inputString;
	if (FFileHelper::LoadFileToString(inputString, *paramFilePath)) {
		TSharedPtr<FJsonObject> jsonObject;
		TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(inputString);
		if (FJsonSerializer::Deserialize(reader, jsonObject) && jsonObject.IsValid()) {
			cycleInfos[0] = jsonObject->GetIntegerField("Cycle1");
			cycleInfos[1] = jsonObject->GetIntegerField("Cycle2");
			cycleInfos[2] = jsonObject->GetIntegerField("Cycle3");
			return true;
		}
	}
	return false;
}

void UQuantitativeTradingCanves::ReCaculateSpecifyIndicator(FString inSpecifyName, const int cycleInfos[3]){
	if (cycleInfos[0] < 2 || cycleInfos[1] < 2 || cycleInfos[2] < 2 ) {
		UE_LOG(LogTemp, Warning, TEXT("Cycle days for calculating indicator %s is invalid!!"), *inSpecifyName);
		return;
	}
	if (!SaveCycleSettingsToJson(inSpecifyName, cycleInfos)) { UE_LOG(LogTemp, Warning, TEXT("Store indicator %s params failed!!"), *inSpecifyName); return; }
	//-----------------------------------------------------更新MACD------------------------------------------------------
	if (inSpecifyName == "DIF1") {//---------------------------------------------------------------------------更新MACD
		//cycleInfos[0]是DIF1的周期,cycleInfos[1]是DIF2的周期,cycleInfos[2]是DEA的周期,
		float dif1Alpha = 2.0f / (static_cast<float>(cycleInfos[0]) + 1.0f);
		float deaAlpha = 2.0f / (static_cast<float>(cycleInfos[2]) + 1.0f);
		for (int i = 1; i < allStockIndexRows.Num(); ++i) {
			allStockIndexRows[i]->DIF1 = allStockIndexRows[i - 1]->DIF1 + dif1Alpha * (allStockIndexRows[i]->Close - allStockIndexRows[i - 1]->DIF1);
			allStockIndexRows[i]->DIF = allStockIndexRows[i]->DIF1 - allStockIndexRows[i]->DIF2;
			allStockIndexRows[i]->DEA = allStockIndexRows[i - 1]->DEA + deaAlpha * (allStockIndexRows[i]->DIF - allStockIndexRows[i - 1]->DEA);
			allStockIndexRows[i]->MACD = (allStockIndexRows[i]->DIF - allStockIndexRows[i]->DEA) * 2.0f;
		}
		return;
	}
	if (inSpecifyName == "DIF2") {//---------------------------------------------------------------------------更新MACD
		//cycleInfos[0]是DIF1的周期,cycleInfos[1]是DIF2的周期,cycleInfos[2]是DEA的周期,
		float dif2Alpha = 2.0f / (static_cast<float>(cycleInfos[1]) + 1.0f);
		float deaAlpha = 2.0f / (static_cast<float>(cycleInfos[2]) + 1.0f);
		for (int i = 1; i < allStockIndexRows.Num(); ++i) {
			allStockIndexRows[i]->DIF2 = allStockIndexRows[i - 1]->DIF2 + dif2Alpha * (allStockIndexRows[i]->Close - allStockIndexRows[i - 1]->DIF2);
			allStockIndexRows[i]->DIF = allStockIndexRows[i]->DIF1 - allStockIndexRows[i]->DIF2;
			allStockIndexRows[i]->DEA = allStockIndexRows[i - 1]->DEA + deaAlpha * (allStockIndexRows[i]->DIF - allStockIndexRows[i - 1]->DEA);
			allStockIndexRows[i]->MACD = (allStockIndexRows[i]->DIF - allStockIndexRows[i]->DEA) * 2.0f;
		}
		return;
	}
	if (inSpecifyName == "DEA") {//---------------------------------------------------------------------------更新MACD
		//cycleInfos[0]是DIF1的周期,cycleInfos[1]是DIF2的周期,cycleInfos[2]是DEA的周期,
		float deaAlpha = 2.0f / (static_cast<float>(cycleInfos[2]) + 1.0f);
		for (int i = 1; i < allStockIndexRows.Num(); ++i) {
			allStockIndexRows[i]->DEA = allStockIndexRows[i - 1]->DEA + deaAlpha * (allStockIndexRows[i]->DIF - allStockIndexRows[i - 1]->DEA);
			allStockIndexRows[i]->MACD = (allStockIndexRows[i]->DIF - allStockIndexRows[i]->DEA) * 2.0f;
		}
		return;
	}
	//-----------------------------------------------------更新KDJ------------------------------------------------------
	if (inSpecifyName == "RSV") {//---------------------------------------------------------------------------更新KDJ
		//cycleInfos[0]是RSV的周期,cycleInfos[1]是K的周期,cycleInfos[2]是D的周期
		float rsvcycle = cycleInfos[0];
		float kcycle = cycleInfos[1];
		float dcycle = cycleInfos[2];
		allStockIndexRows[rsvcycle - 2]->KDJ_K = 50.0f;
		allStockIndexRows[rsvcycle - 2]->KDJ_D = 50.0f;
		for (int i = rsvcycle; i < allStockIndexRows.Num(); ++i) {//从rsvcycle个交易日开始计算
			//获取rsvcycle天内的最高价和最低价
			float highestN = 0.0f;
			float lowestN = FLT_MAX;
			for (int j = 0; j < rsvcycle; ++j) {
				if (allStockIndexRows[i - j]->Close > highestN) highestN = allStockIndexRows[i - j]->Close;
				if (allStockIndexRows[i - j]->Close < lowestN) lowestN = allStockIndexRows[i - j]->Close;
			}
			//计算RSV
			if (highestN == lowestN) allStockIndexRows[i]->KDJ_RSV = 0.0f;
			else allStockIndexRows[i]->KDJ_RSV = (allStockIndexRows[i]->Close - lowestN) / (highestN - lowestN) * 100.0f;
			//计算K值和D值和J值
			allStockIndexRows[i]->KDJ_K = ((kcycle - 1.0f) / kcycle) * allStockIndexRows[i - 1]->KDJ_K + (1.0f / kcycle) * allStockIndexRows[i]->KDJ_RSV;
			allStockIndexRows[i]->KDJ_D = ((dcycle - 1.0f) / dcycle) * allStockIndexRows[i - 1]->KDJ_D + (1.0f / dcycle) * allStockIndexRows[i]->KDJ_K;
			allStockIndexRows[i]->KDJ_J = 3.0f * allStockIndexRows[i]->KDJ_K - 2.0f * allStockIndexRows[i]->KDJ_D;
		}
		return;
	}
	if (inSpecifyName == "K") {//---------------------------------------------------------------------------更新KDJ
		//cycleInfos[0]是RSV的周期,cycleInfos[1]是K的周期,cycleInfos[2]是D的周期
		float rsvcycle = cycleInfos[0];
		float kcycle = cycleInfos[1];
		float dcycle = cycleInfos[2];
		for (int i = rsvcycle; i < allStockIndexRows.Num(); ++i) {//从rsvcycle个交易日开始计算
			//计算K值和D值和J值
			allStockIndexRows[i]->KDJ_K = ((kcycle - 1.0f) / kcycle) * allStockIndexRows[i - 1]->KDJ_K + (1.0f / kcycle) * allStockIndexRows[i]->KDJ_RSV;
			allStockIndexRows[i]->KDJ_D = ((dcycle - 1.0f) / dcycle) * allStockIndexRows[i - 1]->KDJ_D + (1.0f / dcycle) * allStockIndexRows[i]->KDJ_K;
			allStockIndexRows[i]->KDJ_J = 3.0f * allStockIndexRows[i]->KDJ_K - 2.0f * allStockIndexRows[i]->KDJ_D;
		}
		return;
	}
	if (inSpecifyName == "D") {//---------------------------------------------------------------------------更新KDJ
		//cycleInfos[0]是RSV的周期,cycleInfos[1]是K的周期,cycleInfos[2]是D的周期
		float rsvcycle = cycleInfos[0];
		float kcycle = cycleInfos[1];
		float dcycle = cycleInfos[2];
		for (int i = rsvcycle; i < allStockIndexRows.Num(); ++i) {//从rsvcycle个交易日开始计算
			//计算K值和D值和J值
			allStockIndexRows[i]->KDJ_D = ((dcycle - 1.0f) / dcycle) * allStockIndexRows[i - 1]->KDJ_D + (1.0f / dcycle) * allStockIndexRows[i]->KDJ_K;
			allStockIndexRows[i]->KDJ_J = 3.0f * allStockIndexRows[i]->KDJ_K - 2.0f * allStockIndexRows[i]->KDJ_D;
		}
		return;
	}
	//-----------------------------------------------------更新RSI------------------------------------------------------
	if (inSpecifyName == "RSI0") {
		//RSI0的周期,RSI1的周期,RSI2的周期都存放在cycleInfos[0]
		float upSum = 0.0f, downSum = 0.0f;
		for (int i = 1; i < cycleInfos[0]+1; ++i) {
			float changeValue = allStockIndexRows[i]->Close - allStockIndexRows[i - 1]->Close;
			if (changeValue > 0) upSum += changeValue;
			else downSum -= changeValue;
		}
		allStockIndexRows[cycleInfos[0]]->RSI0_AVGUp = upSum / static_cast<float>(cycleInfos[0]);
		allStockIndexRows[cycleInfos[0]]->RSI0_AVGDown = downSum / static_cast<float>(cycleInfos[0]);
		for (int i = cycleInfos[0]; i < allStockIndexRows.Num(); ++i) {
			float changeValue = allStockIndexRows[i]->Close - allStockIndexRows[i - 1]->Close;
			if (changeValue > 0) {
				allStockIndexRows[i]->RSI0_AVGUp = (allStockIndexRows[i - 1]->RSI0_AVGUp * static_cast<float>(cycleInfos[0] - 1) + changeValue) / static_cast<float>(cycleInfos[0]);
				allStockIndexRows[i]->RSI0_AVGDown = (allStockIndexRows[i - 1]->RSI0_AVGDown * static_cast<float>(cycleInfos[0] - 1)) / static_cast<float>(cycleInfos[0]);
			}
			else {
				allStockIndexRows[i]->RSI0_AVGDown = (allStockIndexRows[i - 1]->RSI0_AVGDown * static_cast<float>(cycleInfos[0] - 1) - changeValue) / static_cast<float>(cycleInfos[0]);
				allStockIndexRows[i]->RSI0_AVGUp = (allStockIndexRows[i - 1]->RSI0_AVGUp * static_cast<float>(cycleInfos[0] - 1)) / static_cast<float>(cycleInfos[0]);
			}
			float RS = allStockIndexRows[i]->RSI0_AVGUp / allStockIndexRows[i]->RSI0_AVGDown;
			allStockIndexRows[i]->RSI0 = 100.0f - (100.0f / (1.0f + RS));
		}
		return;
	}
	if (inSpecifyName == "RSI1") {
		//RSI1的周期,RSI1的周期,RSI2的周期都存放在cycleInfos[0]
		float upSum = 0.0f, downSum = 0.0f;
		for (int i = 1; i < cycleInfos[0]+1; ++i) {
			float changeValue = allStockIndexRows[i]->Close - allStockIndexRows[i - 1]->Close;
			if (changeValue > 0) upSum += changeValue;
			else downSum -= changeValue;
		}
		allStockIndexRows[cycleInfos[0]]->RSI1_AVGUp = upSum / static_cast<float>(cycleInfos[0]);
		allStockIndexRows[cycleInfos[0]]->RSI1_AVGDown = downSum / static_cast<float>(cycleInfos[0]);
		for (int i = cycleInfos[0]; i < allStockIndexRows.Num(); ++i) {
			float changeValue = allStockIndexRows[i]->Close - allStockIndexRows[i - 1]->Close;
			if (changeValue > 0) {
				allStockIndexRows[i]->RSI1_AVGUp = (allStockIndexRows[i - 1]->RSI1_AVGUp * static_cast<float>(cycleInfos[0] - 1) + changeValue) / static_cast<float>(cycleInfos[0]);
				allStockIndexRows[i]->RSI1_AVGDown = (allStockIndexRows[i - 1]->RSI1_AVGDown * static_cast<float>(cycleInfos[0] - 1)) / static_cast<float>(cycleInfos[0]);
			}
			else {
				allStockIndexRows[i]->RSI1_AVGDown = (allStockIndexRows[i - 1]->RSI1_AVGDown * static_cast<float>(cycleInfos[0] - 1) - changeValue) / static_cast<float>(cycleInfos[0]);
				allStockIndexRows[i]->RSI1_AVGUp = (allStockIndexRows[i - 1]->RSI1_AVGUp * static_cast<float>(cycleInfos[0] - 1)) / static_cast<float>(cycleInfos[0]);
			}
			float RS = allStockIndexRows[i]->RSI1_AVGUp / allStockIndexRows[i]->RSI1_AVGDown;
			allStockIndexRows[i]->RSI1 = 100.0f - (100.0f / (1.0f + RS));
		}
		return;
	}
	if (inSpecifyName == "RSI2") {
		//RSI2的周期,RSI2的周期,RSI2的周期都存放在cycleInfos[0]
		float upSum = 0.0f, downSum = 0.0f;
		for (int i = 1; i < cycleInfos[0]+1; ++i) {
			float changeValue = allStockIndexRows[i]->Close - allStockIndexRows[i - 1]->Close;
			if (changeValue > 0) upSum += changeValue;
			else downSum -= changeValue;
		}
		allStockIndexRows[cycleInfos[0]]->RSI2_AVGUp = upSum / static_cast<float>(cycleInfos[0]);
		allStockIndexRows[cycleInfos[0]]->RSI2_AVGDown = downSum / static_cast<float>(cycleInfos[0]);
		for (int i = cycleInfos[0]; i < allStockIndexRows.Num(); ++i) {
			float changeValue = allStockIndexRows[i]->Close - allStockIndexRows[i - 1]->Close;
			if (changeValue > 0) {
				allStockIndexRows[i]->RSI2_AVGUp = (allStockIndexRows[i - 1]->RSI2_AVGUp * static_cast<float>(cycleInfos[0] - 1) + changeValue) / static_cast<float>(cycleInfos[0]);
				allStockIndexRows[i]->RSI2_AVGDown = (allStockIndexRows[i - 1]->RSI2_AVGDown * static_cast<float>(cycleInfos[0] - 1)) / static_cast<float>(cycleInfos[0]);
			}
			else {
				allStockIndexRows[i]->RSI2_AVGDown = (allStockIndexRows[i - 1]->RSI2_AVGDown * static_cast<float>(cycleInfos[0] - 1) - changeValue) / static_cast<float>(cycleInfos[0]);
				allStockIndexRows[i]->RSI2_AVGUp = (allStockIndexRows[i - 1]->RSI2_AVGUp * static_cast<float>(cycleInfos[0] - 1)) / static_cast<float>(cycleInfos[0]);
			}
			float RS = allStockIndexRows[i]->RSI2_AVGUp / allStockIndexRows[i]->RSI2_AVGDown;
			allStockIndexRows[i]->RSI2 = 100.0f - (100.0f / (1.0f + RS));
		}
		return;
	}
	//-----------------------------------------------------更新WR------------------------------------------------------
	if (inSpecifyName == "WR1") {
		//WR1的周期, WR2的周期都存放在cycleInfos[0]
		for (int i = cycleInfos[0] - 1; i < allStockIndexRows.Num(); ++i) {
			//获取N个交易日内的最高价和最低价(含当天)
			float highestN = 0.0f;
			float lowestN = FLT_MAX;
			for (int j = 0; j < cycleInfos[0]; ++j) {
				if (allStockIndexRows[i - j]->Close > highestN) highestN = allStockIndexRows[i - j]->Close;
				if (allStockIndexRows[i - j]->Close < lowestN) lowestN = allStockIndexRows[i - j]->Close;
			}
			//计算WR1
			if (highestN == lowestN) allStockIndexRows[i]->WR1 = 0.0f;
			else allStockIndexRows[i]->WR1 = (highestN - allStockIndexRows[i]->Close) / (highestN - lowestN) * 100.0f;
		}
		return;
	}
	if (inSpecifyName == "WR2") {
		//WR1的周期, WR2的周期都存放在cycleInfos[0]
		for (int i = cycleInfos[0] - 1; i < allStockIndexRows.Num(); ++i) {
			//获取N个交易日内的最高价和最低价(含当天)
			float highestN = 0.0f;
			float lowestN = FLT_MAX;
			for (int j = 0; j < cycleInfos[0]; ++j) {
				if (allStockIndexRows[i - j]->Close > highestN) highestN = allStockIndexRows[i - j]->Close;
				if (allStockIndexRows[i - j]->Close < lowestN) lowestN = allStockIndexRows[i - j]->Close;
			}
			//计算WR2
			if (highestN == lowestN) allStockIndexRows[i]->WR2 = 0.0f;
			else allStockIndexRows[i]->WR2 = (highestN - allStockIndexRows[i]->Close) / (highestN - lowestN) * 100.0f;
		}
		return;
	}
	//-----------------------------------------------------更新DMI------------------------------------------------------
	if (inSpecifyName == "PDI" || inSpecifyName == "NDI" || inSpecifyName == "ADX") {
		SetFirstValues_DMI(allStockIndexRows, cycleInfos);
		for (int i = cycleInfos[0] - 1; i < allStockIndexRows.Num(); ++i) {
			CalculateAndStoreDMI(allStockIndexRows, i, cycleInfos);
		}
		return;
	}
	if (inSpecifyName == "ADXR") {
		for (int i = 2 * cycleInfos[0] + cycleInfos[1] - 3; i < allStockIndexRows.Num(); ++i) {
			allStockIndexRows[i]->ADXR = (allStockIndexRows[i]->ADX + allStockIndexRows[i - cycleInfos[1] + 1]->ADX) * 0.5f;
		}
		return;
	}
	//-----------------------------------------------------更新CCI------------------------------------------------------
	if (inSpecifyName == "CCI") {
		SetFirstValues_CCI(allStockIndexRows, cycleInfos);
		for (int i = cycleInfos[0]; i < allStockIndexRows.Num(); ++i) {
			CalculateAndStoreCCI(allStockIndexRows, i, cycleInfos);
		}
		return;
	}
	//-----------------------------------------------------更新BIAS------------------------------------------------------
	if (inSpecifyName == "BIAS0") {
		SetFirstValues_BIAS(allStockIndexRows, cycleInfos, 0);
		for (int i = cycleInfos[0]; i < allStockIndexRows.Num(); ++i) {
			CalculateAndStoreBIAS(allStockIndexRows, i, cycleInfos, 0);
		}
		return;
	}
	if (inSpecifyName == "BIAS1") {
		SetFirstValues_BIAS(allStockIndexRows, cycleInfos, 1);
		for (int i = cycleInfos[0]; i < allStockIndexRows.Num(); ++i) {
			CalculateAndStoreBIAS(allStockIndexRows, i, cycleInfos, 1);
		}
		return;
	}
	if (inSpecifyName == "BIAS2") {
		SetFirstValues_BIAS(allStockIndexRows, cycleInfos, 2);
		for (int i = cycleInfos[0]; i < allStockIndexRows.Num(); ++i) {
			CalculateAndStoreBIAS(allStockIndexRows, i, cycleInfos, 2);
		}
		return;
	}
}

void UQuantitativeTradingCanves::ReCaculateAndStoreLatestDayKLine(const FQTStockRealTimeData& inRealTimeData){
	if (allStockIndexRows.Num() == 0) return;
	TSharedPtr<FQTStockIndex>& latestRow = allStockIndexRows.Last();
	latestRow->Open = inRealTimeData.OpenPrice;
	latestRow->Close = inRealTimeData.LatestPrice;
	latestRow->High = inRealTimeData.HighestPrice;
	latestRow->Low = inRealTimeData.LowestPrice;
	latestRow->Change = inRealTimeData.ChangeAmount;
	latestRow->ChangeRatio = inRealTimeData.ChangeRatio;
	latestRow->Volume = inRealTimeData.Volume;
	latestRow->Turnover = inRealTimeData.Turnover;
	latestRow->PriceRange = inRealTimeData.PriceRange;
	latestRow->TurnoverRate = inRealTimeData.TurnoverRate;
	//更新MA值
	int i = allStockIndexRows.Num() - 1;
	{//--------->>计算SMA
		//计算SMA5
		allStockIndexRows[i]->SMA5SUM = allStockIndexRows[i - 1]->SMA5SUM - allStockIndexRows[i - 5]->Close + allStockIndexRows[i]->Close;
		allStockIndexRows[i]->SMA5 = allStockIndexRows[i]->SMA5SUM / 5.0f;
		//--------->>计算SMA10
		allStockIndexRows[i]->SMA10SUM = allStockIndexRows[i - 1]->SMA10SUM - allStockIndexRows[i - 10]->Close + allStockIndexRows[i]->Close;
		allStockIndexRows[i]->SMA10 = allStockIndexRows[i]->SMA10SUM / 10.0f;
		//--------->>计算SMA20
		allStockIndexRows[i]->SMA20SUM = allStockIndexRows[i - 1]->SMA20SUM - allStockIndexRows[i - 20]->Close + allStockIndexRows[i]->Close;
		allStockIndexRows[i]->SMA20 = allStockIndexRows[i]->SMA20SUM / 20.0f;
		//--------->>计算SMA60
		allStockIndexRows[i]->SMA60SUM = allStockIndexRows[i - 1]->SMA60SUM - allStockIndexRows[i - 60]->Close + allStockIndexRows[i]->Close;
		allStockIndexRows[i]->SMA60 = allStockIndexRows[i]->SMA60SUM / 60.0f;
		//--------->>计算SMA240
		allStockIndexRows[i]->SMA240SUM = allStockIndexRows[i - 1]->SMA240SUM - allStockIndexRows[i - 240]->Close + allStockIndexRows[i]->Close;
		allStockIndexRows[i]->SMA240 = allStockIndexRows[i]->SMA240SUM / 240.0f;
	}
	{//--------->>计算EMA
		float alpha5 = 2.0f / (5 + 1);
		float alpha9 = 2.0f / (9 + 1);
		float alpha10 = 2.0f / (10 + 1);
		float alpha12 = 2.0f / (12 + 1);
		float alpha20 = 2.0f / (20 + 1);
		float alpha26 = 2.0f / (26 + 1);
		float alpha60 = 2.0f / (60 + 1);
		float alpha240 = 2.0f / (240 + 1);
		allStockIndexRows[i]->EMA5 = allStockIndexRows[i - 1]->EMA5 + alpha5 * (allStockIndexRows[i]->Close - allStockIndexRows[i - 1]->EMA5);
		allStockIndexRows[i]->EMA10 = allStockIndexRows[i - 1]->EMA10 + alpha10 * (allStockIndexRows[i]->Close - allStockIndexRows[i - 1]->EMA10);
		allStockIndexRows[i]->EMA20 = allStockIndexRows[i - 1]->EMA20 + alpha20 * (allStockIndexRows[i]->Close - allStockIndexRows[i - 1]->EMA20);
		allStockIndexRows[i]->EMA60 = allStockIndexRows[i - 1]->EMA60 + alpha60 * (allStockIndexRows[i]->Close - allStockIndexRows[i - 1]->EMA60);
		allStockIndexRows[i]->EMA240 = allStockIndexRows[i - 1]->EMA240 + alpha240 * (allStockIndexRows[i]->Close - allStockIndexRows[i - 1]->EMA240);
	}
	//更新各种指标
	int cycleInfos[3];
	{//更新MACD
		LoadCycleSettingsFromJson("MACD", cycleInfos);//cycleInfos[0]是DIF1的周期,cycleInfos[1]是DIF2的周期,cycleInfos[2]是DEA的周期
		float dif1Alpha = 2.0f / (static_cast<float>(cycleInfos[0]) + 1.0f);
		float dif2Alpha = 2.0f / (static_cast<float>(cycleInfos[1]) + 1.0f);
		float deaAlpha = 2.0f / (static_cast<float>(cycleInfos[2]) + 1.0f);
		allStockIndexRows[i]->DIF1 = allStockIndexRows[i - 1]->DIF1 + dif1Alpha * (allStockIndexRows[i]->Close - allStockIndexRows[i - 1]->DIF1);
		allStockIndexRows[i]->DIF2 = allStockIndexRows[i - 1]->DIF2 + dif2Alpha * (allStockIndexRows[i]->Close - allStockIndexRows[i - 1]->DIF2);
		allStockIndexRows[i]->DIF = allStockIndexRows[i]->DIF1 - allStockIndexRows[i]->DIF2;
		allStockIndexRows[i]->DEA = allStockIndexRows[i - 1]->DEA + deaAlpha * (allStockIndexRows[i]->DIF - allStockIndexRows[i - 1]->DEA);
		allStockIndexRows[i]->MACD = (allStockIndexRows[i]->DIF - allStockIndexRows[i]->DEA) * 2.0f;
	}
	{//更新KDJ
		LoadCycleSettingsFromJson("KDJ", cycleInfos);//RSV的周期存储在cycleInfos[0]，K值周期存储在cycleInfos[1]，D值周期存储在cycleInfos[2]
		float rsvcycle = cycleInfos[0];
		float kcycle = cycleInfos[1];
		float dcycle = cycleInfos[2];
		//获取rsvcycle天内的最高价和最低价
		float highestN = 0.0f;
		float lowestN = FLT_MAX;
		for (int j = 0; j < rsvcycle; ++j) {
			if (allStockIndexRows[i - j]->Close > highestN) highestN = allStockIndexRows[i - j]->Close;
			if (allStockIndexRows[i - j]->Close < lowestN) lowestN = allStockIndexRows[i - j]->Close;
		}
		//计算RSV
		if (highestN == lowestN) allStockIndexRows[i]->KDJ_RSV = 0.0f;
		else allStockIndexRows[i]->KDJ_RSV = (allStockIndexRows[i]->Close - lowestN) / (highestN - lowestN) * 100.0f;
		//计算K值和D值和J值
		allStockIndexRows[i]->KDJ_K = ((kcycle - 1.0f) / kcycle) * allStockIndexRows[i - 1]->KDJ_K + (1.0f / kcycle) * allStockIndexRows[i]->KDJ_RSV;
		allStockIndexRows[i]->KDJ_D = ((dcycle - 1.0f) / dcycle) * allStockIndexRows[i - 1]->KDJ_D + (1.0f / dcycle) * allStockIndexRows[i]->KDJ_K;
		allStockIndexRows[i]->KDJ_J = 3.0f * allStockIndexRows[i]->KDJ_K - 2.0f * allStockIndexRows[i]->KDJ_D;
	}
	{//更新RSI
		LoadCycleSettingsFromJson("RSI", cycleInfos);//RSI0的周期存储在cycleInfos[0]，RSI1的周期存储在cycleInfos[1],RSI2的周期存储在cycleInfos[2]
		float changeValue = allStockIndexRows[i]->Close - allStockIndexRows[i - 1]->Close;
		if (changeValue > 0) {
			allStockIndexRows[i]->RSI0_AVGUp = (allStockIndexRows[i - 1]->RSI0_AVGUp * static_cast<float>(cycleInfos[0] - 1) + changeValue) / static_cast<float>(cycleInfos[0]);
			allStockIndexRows[i]->RSI0_AVGDown = (allStockIndexRows[i - 1]->RSI0_AVGDown * static_cast<float>(cycleInfos[0] - 1)) / static_cast<float>(cycleInfos[0]);
			allStockIndexRows[i]->RSI1_AVGUp = (allStockIndexRows[i - 1]->RSI1_AVGUp * static_cast<float>(cycleInfos[1] - 1) + changeValue) / static_cast<float>(cycleInfos[1]);
			allStockIndexRows[i]->RSI1_AVGDown = (allStockIndexRows[i - 1]->RSI1_AVGDown * static_cast<float>(cycleInfos[1] - 1)) / static_cast<float>(cycleInfos[1]);
			allStockIndexRows[i]->RSI2_AVGUp = (allStockIndexRows[i - 1]->RSI2_AVGUp * static_cast<float>(cycleInfos[2] - 1) + changeValue) / static_cast<float>(cycleInfos[2]);
			allStockIndexRows[i]->RSI2_AVGDown = (allStockIndexRows[i - 1]->RSI2_AVGDown * static_cast<float>(cycleInfos[2] - 1)) / static_cast<float>(cycleInfos[2]);
		}
		else {
			allStockIndexRows[i]->RSI0_AVGDown = (allStockIndexRows[i - 1]->RSI0_AVGDown * static_cast<float>(cycleInfos[0] - 1) - changeValue) / static_cast<float>(cycleInfos[0]);
			allStockIndexRows[i]->RSI0_AVGUp = (allStockIndexRows[i - 1]->RSI0_AVGUp * static_cast<float>(cycleInfos[0] - 1)) / static_cast<float>(cycleInfos[0]);
			allStockIndexRows[i]->RSI1_AVGDown = (allStockIndexRows[i - 1]->RSI1_AVGDown * static_cast<float>(cycleInfos[1] - 1) - changeValue) / static_cast<float>(cycleInfos[1]);
			allStockIndexRows[i]->RSI1_AVGUp = (allStockIndexRows[i - 1]->RSI1_AVGUp * static_cast<float>(cycleInfos[1] - 1)) / static_cast<float>(cycleInfos[1]);
			allStockIndexRows[i]->RSI2_AVGDown = (allStockIndexRows[i - 1]->RSI2_AVGDown * static_cast<float>(cycleInfos[2] - 1) - changeValue) / static_cast<float>(cycleInfos[2]);
			allStockIndexRows[i]->RSI2_AVGUp = (allStockIndexRows[i - 1]->RSI2_AVGUp * static_cast<float>(cycleInfos[2] - 1)) / static_cast<float>(cycleInfos[2]);
		}
		float RS0 = allStockIndexRows[i]->RSI0_AVGUp / allStockIndexRows[i]->RSI0_AVGDown;
		allStockIndexRows[i]->RSI0 = 100.0f - (100.0f / (1.0f + RS0));
		float RS1 = allStockIndexRows[i]->RSI1_AVGUp / allStockIndexRows[i]->RSI1_AVGDown;
		allStockIndexRows[i]->RSI1 = 100.0f - (100.0f / (1.0f + RS1));
		float RS2 = allStockIndexRows[i]->RSI2_AVGUp / allStockIndexRows[i]->RSI2_AVGDown;
		allStockIndexRows[i]->RSI2 = 100.0f - (100.0f / (1.0f + RS2));
	}
	{//更新WR
		LoadCycleSettingsFromJson("WR", cycleInfos);//WR1的周期存储在cycleInfos[0],WR2的周期存储在cycleInfos[1]
		//获取N个交易日内的最高价和最低价(含当天)
		//计算WR1
		float highestN = 0.0f;
		float lowestN = FLT_MAX;
		for (int j = 0; j < cycleInfos[0]; ++j) {
			if (allStockIndexRows[i - j]->Close > highestN) highestN = allStockIndexRows[i - j]->Close;
			if (allStockIndexRows[i - j]->Close < lowestN) lowestN = allStockIndexRows[i - j]->Close;
		}
		if (highestN == lowestN) allStockIndexRows[i]->WR1 = 0.0f;
		else allStockIndexRows[i]->WR1 = (highestN - allStockIndexRows[i]->Close) / (highestN - lowestN) * 100.0f;
		//计算WR2
		highestN = 0.0f;
		lowestN = FLT_MAX;
		for (int j = 0; j < cycleInfos[1]; ++j) {
			if (allStockIndexRows[i - j]->Close > highestN) highestN = allStockIndexRows[i - j]->Close;
			if (allStockIndexRows[i - j]->Close < lowestN) lowestN = allStockIndexRows[i - j]->Close;
		}
		//计算WR2
		if (highestN == lowestN) allStockIndexRows[i]->WR2 = 0.0f;
		else allStockIndexRows[i]->WR2 = (highestN - allStockIndexRows[i]->Close) / (highestN - lowestN) * 100.0f;
	}
	{//更新DMI
		LoadCycleSettingsFromJson("DMI", cycleInfos);//前三个参数周期保持一致，都存储在cycleInfos[0],ADXR周期存储在cycleInfos[1]
		CalculateAndStoreDMI(allStockIndexRows, i, cycleInfos);
	}
	{//更新CCI
		LoadCycleSettingsFromJson("CCI", cycleInfos);//CCI只有一个，周期存储在cycleInfos[0]
		CalculateAndStoreCCI(allStockIndexRows, i, cycleInfos);
	}
	{//更新BIAS
		LoadCycleSettingsFromJson("BIAS", cycleInfos);//cycleInfos分别存储BIAS0，BIAS1，BIAS2的周期
		CalculateAndStoreBIAS(allStockIndexRows, i, cycleInfos, 0);
		CalculateAndStoreBIAS(allStockIndexRows, i, cycleInfos, 1);
		CalculateAndStoreBIAS(allStockIndexRows, i, cycleInfos, 2);
	}
}

void UQuantitativeTradingCanves::RefreshVisibleRows(){
	GetIntervalRow(allStockIndexRows, visibleRows, startDate, currentDate);
}

void UQuantitativeTradingCanves::ReSampleSpecifyIndicator(FString inSpecifyName){
	float timeRange = visibleRows.Num() + 1;
	float timeStep = timeRange / (samplingCounts - 1);
	//-----------------------计算MACD
	if (inSpecifyName == "DIF1" || inSpecifyName == "DIF2" || inSpecifyName == "DEA") {
		absmaxMACD = 0.0f;
		for (auto item : visibleRows) {
			if (FMath::Abs(item->MACD) > absmaxMACD) { absmaxMACD = FMath::Abs(item->MACD); }
			if (FMath::Abs(item->DEA) > absmaxMACD) { absmaxMACD = FMath::Abs(item->DEA); }
			if (FMath::Abs(item->DIF) > absmaxMACD) { absmaxMACD = FMath::Abs(item->DIF); }
		}
		BroadcastIndicatorValueRangeByIndicatorName(indicatorName);
		MACDUnitPoints.Empty();
		MACDUnitPoints.Reserve(visibleRows.Num());
		for (int i = 0; i < visibleRows.Num(); ++i) {
			float unitMACD = (1.f - (visibleRows[i]->MACD / absmaxMACD * 0.5f + 0.5f)) * 0.2f + 0.8f;
			float unitDIF = (1.f - (visibleRows[i]->DIF / absmaxMACD * 0.5f + 0.5f)) * 0.2f + 0.8f;
			float unitDEA = (1.f - (visibleRows[i]->DEA / absmaxMACD * 0.5f + 0.5f)) * 0.2f + 0.8f;
			MACDUnitPoints.Add(FVector3f(unitDIF, unitDEA, unitMACD));
		}
		return;
	}
	//-----------------------计算KDJ
	if (inSpecifyName == "RSV" || inSpecifyName == "K" || inSpecifyName == "D") {
		minKDJ = MAX_FLT, maxKDJ = -MAX_FLT;
		for (auto item : visibleRows) {
			if (item->KDJ_K > maxKDJ) maxKDJ = item->KDJ_K;
			if (item->KDJ_D > maxKDJ) maxKDJ = item->KDJ_D;
			if (item->KDJ_J > maxKDJ) maxKDJ = item->KDJ_J;
			if (item->KDJ_K < minKDJ) minKDJ = item->KDJ_K;
			if (item->KDJ_D < minKDJ) minKDJ = item->KDJ_D;
			if (item->KDJ_J < minKDJ) minKDJ = item->KDJ_J;
		}
		BroadcastIndicatorValueRangeByIndicatorName(indicatorName);
		float KDJValueRange = maxKDJ - minKDJ;
		kdjOutLines.X = ((1.f - (100.0f - minKDJ) / KDJValueRange) * 0.2f + 0.8f);
		kdjOutLines.Y = ((1.f - (80.0f - minKDJ) / KDJValueRange) * 0.2f + 0.8f);
		kdjOutLines.Z = ((1.f - (20.0f - minKDJ) / KDJValueRange) * 0.2f + 0.8f);
		kdjOutLines.W = ((1.f - (0.0f - minKDJ) / KDJValueRange) * 0.2f + 0.8f);
		KDJUnitPoints.Empty();
		KDJUnitPoints.Reserve(visibleRows.Num());
		for (int i = 0; i < visibleRows.Num(); ++i) {
			KDJUnitPoints.Add(FVector3f((1.f - (visibleRows[i]->KDJ_K - minKDJ) / KDJValueRange) * 0.2f + 0.8f,
				(1.f - (visibleRows[i]->KDJ_D - minKDJ) / KDJValueRange) * 0.2f + 0.8f,
				(1.f - (visibleRows[i]->KDJ_J - minKDJ) / KDJValueRange) * 0.2f + 0.8f));
		}
		return;
	}
	//-----------------------计算RSI
	if (inSpecifyName == "RSI0" || inSpecifyName == "RSI1" || inSpecifyName == "RSI2") {
		minRSI = MAX_FLT, maxRSI = -MAX_FLT;
		for (auto item : visibleRows) {
			if (item->RSI0 > maxRSI) maxRSI = item->RSI0;
			if (item->RSI1 > maxRSI) maxRSI = item->RSI1;
			if (item->RSI2 > maxRSI) maxRSI = item->RSI2;
			if (item->RSI0 < minRSI) minRSI = item->RSI0;
			if (item->RSI1 < minRSI) minRSI = item->RSI1;
			if (item->RSI2 < minRSI) minRSI = item->RSI2;
		}
		BroadcastIndicatorValueRangeByIndicatorName(indicatorName);
		float RSIValueRange = maxRSI - minRSI;
		rsiOutLines.X = ((1.f - (70.0f - minRSI) / RSIValueRange) * 0.2f + 0.8f);
		rsiOutLines.Y = ((1.f - (30.0f - minRSI) / RSIValueRange) * 0.2f + 0.8f);
		RSIUnitPoints0.Empty();
		RSIUnitPoints1.Empty();
		RSIUnitPoints2.Empty();
		RSIUnitPoints0.Reserve(visibleRows.Num());
		RSIUnitPoints1.Reserve(visibleRows.Num());
		RSIUnitPoints2.Reserve(visibleRows.Num());
		for (int i = 0; i < visibleRows.Num(); ++i) {
			RSIUnitPoints0.Add((1.f - (visibleRows[i]->RSI0 - minRSI) / RSIValueRange) * 0.2f + 0.8f);
			RSIUnitPoints1.Add((1.f - (visibleRows[i]->RSI1 - minRSI) / RSIValueRange) * 0.2f + 0.8f);
			RSIUnitPoints2.Add((1.f - (visibleRows[i]->RSI2 - minRSI) / RSIValueRange) * 0.2f + 0.8f);
		}
		return;
	}
	//-----------------------计算WR
	if (inSpecifyName == "WR1" || inSpecifyName == "WR2") {
		minWR = MAX_FLT, maxWR = 0.0f;
		for (auto item : visibleRows) {
			if (item->WR1 > maxWR) maxWR = item->WR1;
			if (item->WR2 > maxWR) maxWR = item->WR2;
			if (item->WR1 < minWR) minWR = item->WR1;
			if (item->WR2 < minWR) minWR = item->WR2;
		}
		BroadcastIndicatorValueRangeByIndicatorName(indicatorName);
		float WRValueRange = maxWR - minWR;
		wrOutLines.X = (((80.0f - minWR) / WRValueRange) * 0.2f + 0.8f);
		wrOutLines.Y = (((20.0f - minWR) / WRValueRange) * 0.2f + 0.8f);
		WRUnitPoints1.Empty();
		WRUnitPoints2.Empty();
		WRUnitPoints1.Reserve(visibleRows.Num());
		WRUnitPoints2.Reserve(visibleRows.Num());
		for (int i = 0; i < visibleRows.Num(); ++i) {
			WRUnitPoints1.Add(((visibleRows[i]->WR1 - minWR) / WRValueRange) * 0.2f + 0.8f);
			WRUnitPoints2.Add(((visibleRows[i]->WR2 - minWR) / WRValueRange) * 0.2f + 0.8f);
		}
		return;
	}
	//-----------------------计算DMI
	if (inSpecifyName == "PDI" || inSpecifyName == "NDI" || inSpecifyName == "ADX" || inSpecifyName == "ADXR") {
		minDMI = 100.0f, maxDMI = 0.0f;
		for (auto item : visibleRows) {
			if (item->PDI > maxDMI) maxDMI = item->PDI;
			if (item->PDI < minDMI) minDMI = item->PDI;
			if (item->NDI > maxDMI) maxDMI = item->NDI;
			if (item->NDI < minDMI) minDMI = item->NDI;
			if (item->ADX > maxDMI) maxDMI = item->ADX;
			if (item->ADX < minDMI) minDMI = item->ADX;
			if (item->ADXR > maxDMI) maxDMI = item->ADXR;
			if (item->ADXR < minDMI) minDMI = item->ADXR;
		}
		BroadcastIndicatorValueRangeByIndicatorName(indicatorName);
		float DMIValueRange = maxDMI - minDMI;
		dmiOutLines.X = ((1.f - (50.0f - minDMI) / DMIValueRange) * 0.2f + 0.8f);
		dmiOutLines.Y = ((1.f - (20.0f - minDMI) / DMIValueRange) * 0.2f + 0.8f);
		DMIUnitPoints.Empty();
		DMIUnitPoints.Reserve(visibleRows.Num());
		for(int i=0; i< visibleRows.Num(); ++i){
			DMIUnitPoints.Add(FVector4f((1.f - (visibleRows[i]->PDI - minDMI) / DMIValueRange) * 0.2f + 0.8f,
				(1.f - (visibleRows[i]->NDI - minDMI) / DMIValueRange) * 0.2f + 0.8f,
				(1.f - (visibleRows[i]->ADX - minDMI) / DMIValueRange) * 0.2f + 0.8f,
				(1.f - (visibleRows[i]->ADXR - minDMI) / DMIValueRange) * 0.2f + 0.8f));
		}
		return;
	}
	//-----------------------计算CCI
	if (inSpecifyName == "CCI") {
		minCCI = MAX_FLT, maxCCI = -MAX_FLT;
		for (auto item : visibleRows) {
			if (item->CCI > maxCCI) maxCCI = item->CCI;
			if (item->CCI < minCCI) minCCI = item->CCI;
		}
		BroadcastIndicatorValueRangeByIndicatorName(indicatorName);
		float CCIValueRange = maxCCI - minCCI;
		cciOutLines.X = ((1.f - (100.0f - minCCI) / CCIValueRange) * 0.2f + 0.8f);
		cciOutLines.Y = ((1.f - (-100.0f - minCCI) / CCIValueRange) * 0.2f + 0.8f);
		CCIUnitPoints.Empty();
		CCIUnitPoints.Reserve(visibleRows.Num());
		for (int i = 0; i < visibleRows.Num(); ++i) {
			CCIUnitPoints.Add((1.f - (visibleRows[i]->CCI - minCCI) / CCIValueRange) * 0.2f + 0.8f);
		}
		return;
	}
	//-----------------------计算BIAS
	if (inSpecifyName == "BIAS0" || inSpecifyName == "BIAS1" || inSpecifyName == "BIAS2") {
		minBIAS = MAX_FLT, maxBIAS = -MAX_FLT;
		for (auto item : visibleRows) {
			if (item->BIAS0 > maxBIAS) maxBIAS = item->BIAS0;
			if (item->BIAS1 > maxBIAS) maxBIAS = item->BIAS1;
			if (item->BIAS2 > maxBIAS) maxBIAS = item->BIAS2;
			if (item->BIAS0 < minBIAS) minBIAS = item->BIAS0;
			if (item->BIAS1 < minBIAS) minBIAS = item->BIAS1;
			if (item->BIAS2 < minBIAS) minBIAS = item->BIAS2;
		}
		BroadcastIndicatorValueRangeByIndicatorName(indicatorName);
		float BIASValueRange = maxBIAS - minBIAS;
		biasOutLines.X = ((1.f - (70.0f - minBIAS) / BIASValueRange) * 0.2f + 0.8f);
		biasOutLines.Y = ((1.f - (30.0f - minBIAS) / BIASValueRange) * 0.2f + 0.8f);
		BIASUnitPoints0.Empty();
		BIASUnitPoints1.Empty();
		BIASUnitPoints2.Empty();
		BIASUnitPoints0.Reserve(visibleRows.Num());
		BIASUnitPoints1.Reserve(visibleRows.Num());
		BIASUnitPoints2.Reserve(visibleRows.Num());
		for (int i = 0; i < visibleRows.Num(); ++i) {
			BIASUnitPoints0.Add((1.f - (visibleRows[i]->BIAS0 - minBIAS) / BIASValueRange) * 0.2f + 0.8f);
			BIASUnitPoints1.Add((1.f - (visibleRows[i]->BIAS1 - minBIAS) / BIASValueRange) * 0.2f + 0.8f);
			BIASUnitPoints2.Add((1.f - (visibleRows[i]->BIAS2 - minBIAS) / BIASValueRange) * 0.2f + 0.8f);
		}
		return;
	}
}

void UQuantitativeTradingCanves::ReSampleIndicatorName(const FString& inIndicatorName){
	float timeRange = visibleRows.Num() + 1;
	float timeStep = timeRange / (samplingCounts - 1);
	//-----------------------计算MACD
	if (inIndicatorName == "MACD") {
		absmaxMACD = 0.0f;
		for (auto item : visibleRows) {
			if (FMath::Abs(item->MACD) > absmaxMACD) { absmaxMACD = FMath::Abs(item->MACD); }
			if (FMath::Abs(item->DEA) > absmaxMACD) { absmaxMACD = FMath::Abs(item->DEA); }
			if (FMath::Abs(item->DIF) > absmaxMACD) { absmaxMACD = FMath::Abs(item->DIF); }
		}
		BroadcastIndicatorValueRangeByIndicatorName(indicatorName);
		MACDUnitPoints.Empty();
		MACDUnitPoints.Reserve(visibleRows.Num());
		for (int i = 0; i < visibleRows.Num(); ++i) {
			float unitMACD = (1.f - (visibleRows[i]->MACD / absmaxMACD * 0.5f + 0.5f)) * 0.2f + 0.8f;
			float unitDIF = (1.f - (visibleRows[i]->DIF / absmaxMACD * 0.5f + 0.5f)) * 0.2f + 0.8f;
			float unitDEA = (1.f - (visibleRows[i]->DEA / absmaxMACD * 0.5f + 0.5f)) * 0.2f + 0.8f;
			MACDUnitPoints.Add(FVector3f(unitDIF, unitDEA, unitMACD));
		}
		return;
	}
	//-----------------------计算KDJ
	if (inIndicatorName == "KDJ") {
		minKDJ = MAX_FLT, maxKDJ = -MAX_FLT;
		for (auto item : visibleRows) {
			if (item->KDJ_K > maxKDJ) maxKDJ = item->KDJ_K;
			if (item->KDJ_D > maxKDJ) maxKDJ = item->KDJ_D;
			if (item->KDJ_J > maxKDJ) maxKDJ = item->KDJ_J;
			if (item->KDJ_K < minKDJ) minKDJ = item->KDJ_K;
			if (item->KDJ_D < minKDJ) minKDJ = item->KDJ_D;
			if (item->KDJ_J < minKDJ) minKDJ = item->KDJ_J;
		}
		BroadcastIndicatorValueRangeByIndicatorName(indicatorName);
		float KDJValueRange = maxKDJ - minKDJ;
		kdjOutLines.X = ((1.f - (100.0f - minKDJ) / KDJValueRange) * 0.2f + 0.8f);
		kdjOutLines.Y = ((1.f - (80.0f - minKDJ) / KDJValueRange) * 0.2f + 0.8f);
		kdjOutLines.Z = ((1.f - (20.0f - minKDJ) / KDJValueRange) * 0.2f + 0.8f);
		kdjOutLines.W = ((1.f - (0.0f - minKDJ) / KDJValueRange) * 0.2f + 0.8f);
		KDJUnitPoints.Empty();
		KDJUnitPoints.Reserve(visibleRows.Num());
		for (int i = 0; i < visibleRows.Num(); ++i) {
			KDJUnitPoints.Add(FVector3f((1.f - (visibleRows[i]->KDJ_K - minKDJ) / KDJValueRange) * 0.2f + 0.8f,
				(1.f - (visibleRows[i]->KDJ_D - minKDJ) / KDJValueRange) * 0.2f + 0.8f,
				(1.f - (visibleRows[i]->KDJ_J - minKDJ) / KDJValueRange) * 0.2f + 0.8f));
		}
		return;
	}
	//-----------------------计算RSI
	if (inIndicatorName == "RSI") {
		minRSI = MAX_FLT, maxRSI = -MAX_FLT;
		for (auto item : visibleRows) {
			if (item->RSI0 > maxRSI) maxRSI = item->RSI0;
			if (item->RSI1 > maxRSI) maxRSI = item->RSI1;
			if (item->RSI2 > maxRSI) maxRSI = item->RSI2;
			if (item->RSI0 < minRSI) minRSI = item->RSI0;
			if (item->RSI1 < minRSI) minRSI = item->RSI1;
			if (item->RSI2 < minRSI) minRSI = item->RSI2;
		}
		BroadcastIndicatorValueRangeByIndicatorName(indicatorName);
		float RSIValueRange = maxRSI - minRSI;
		rsiOutLines.X = ((1.f - (70.0f - minRSI) / RSIValueRange) * 0.2f + 0.8f);
		rsiOutLines.Y = ((1.f - (30.0f - minRSI) / RSIValueRange) * 0.2f + 0.8f);
		RSIUnitPoints0.Empty();
		RSIUnitPoints1.Empty();
		RSIUnitPoints2.Empty();
		RSIUnitPoints0.Reserve(visibleRows.Num());
		RSIUnitPoints1.Reserve(visibleRows.Num());
		RSIUnitPoints2.Reserve(visibleRows.Num());
		for (int i = 0; i < visibleRows.Num(); ++i) {
			RSIUnitPoints0.Add((1.f - (visibleRows[i]->RSI0 - minRSI) / RSIValueRange) * 0.2f + 0.8f);
			RSIUnitPoints1.Add((1.f - (visibleRows[i]->RSI1 - minRSI) / RSIValueRange) * 0.2f + 0.8f);
			RSIUnitPoints2.Add((1.f - (visibleRows[i]->RSI2 - minRSI) / RSIValueRange) * 0.2f + 0.8f);
		}
		return;
	}
	//-----------------------计算WR
	if (inIndicatorName == "WR") {
		minWR = MAX_FLT, maxWR = 0.0f;
		for (auto item : visibleRows) {
			if (item->WR1 > maxWR) maxWR = item->WR1;
			if (item->WR2 > maxWR) maxWR = item->WR2;
			if (item->WR1 < minWR) minWR = item->WR1;
			if (item->WR2 < minWR) minWR = item->WR2;
		}
		BroadcastIndicatorValueRangeByIndicatorName(indicatorName);
		float WRValueRange = maxWR - minWR;
		wrOutLines.X = (((80.0f - minWR) / WRValueRange) * 0.2f + 0.8f);
		wrOutLines.Y = (((20.0f - minWR) / WRValueRange) * 0.2f + 0.8f);
		WRUnitPoints1.Empty();
		WRUnitPoints2.Empty();
		WRUnitPoints1.Reserve(visibleRows.Num());
		WRUnitPoints2.Reserve(visibleRows.Num());
		for (int i = 0; i < visibleRows.Num(); ++i) {
			WRUnitPoints1.Add(((visibleRows[i]->WR1 - minWR) / WRValueRange) * 0.2f + 0.8f);
			WRUnitPoints2.Add(((visibleRows[i]->WR2 - minWR) / WRValueRange) * 0.2f + 0.8f);
		}
		return;
	}
	//-----------------------计算DMI
	if (inIndicatorName == "DMI") {
		minDMI = 100.0f, maxDMI = 0.0f;
		for (auto item : visibleRows) {
			if (item->PDI > maxDMI) maxDMI = item->PDI;
			if (item->PDI < minDMI) minDMI = item->PDI;
			if (item->NDI > maxDMI) maxDMI = item->NDI;
			if (item->NDI < minDMI) minDMI = item->NDI;
			if (item->ADX > maxDMI) maxDMI = item->ADX;
			if (item->ADX < minDMI) minDMI = item->ADX;
			if (item->ADXR > maxDMI) maxDMI = item->ADXR;
			if (item->ADXR < minDMI) minDMI = item->ADXR;
		}
		BroadcastIndicatorValueRangeByIndicatorName(indicatorName);
		float DMIValueRange = maxDMI - minDMI;
		dmiOutLines.X = ((1.f - (50.0f - minDMI) / DMIValueRange) * 0.2f + 0.8f);
		dmiOutLines.Y = ((1.f - (20.0f - minDMI) / DMIValueRange) * 0.2f + 0.8f);
		DMIUnitPoints.Empty();
		DMIUnitPoints.Reserve(visibleRows.Num());
		for (int i = 0; i < visibleRows.Num(); ++i) {
			DMIUnitPoints.Add(FVector4f((1.f - (visibleRows[i]->PDI - minDMI) / DMIValueRange) * 0.2f + 0.8f,
				(1.f - (visibleRows[i]->NDI - minDMI) / DMIValueRange) * 0.2f + 0.8f,
				(1.f - (visibleRows[i]->ADX - minDMI) / DMIValueRange) * 0.2f + 0.8f,
				(1.f - (visibleRows[i]->ADXR - minDMI) / DMIValueRange) * 0.2f + 0.8f));
		}
		return;
	}
	//-----------------------计算CCI
	if (inIndicatorName == "CCI") {
		minCCI = MAX_FLT, maxCCI = -MAX_FLT;
		for (auto item : visibleRows) {
			if (item->CCI > maxCCI) maxCCI = item->CCI;
			if (item->CCI < minCCI) minCCI = item->CCI;
		}
		BroadcastIndicatorValueRangeByIndicatorName(indicatorName);
		float CCIValueRange = maxCCI - minCCI;
		cciOutLines.X = ((1.f - (100.0f - minCCI) / CCIValueRange) * 0.2f + 0.8f);
		cciOutLines.Y = ((1.f - (-100.0f - minCCI) / CCIValueRange) * 0.2f + 0.8f);
		CCIUnitPoints.Empty();
		CCIUnitPoints.Reserve(visibleRows.Num());
		for (int i = 0; i < visibleRows.Num(); ++i) {
			CCIUnitPoints.Add((1.f - (visibleRows[i]->CCI - minCCI) / CCIValueRange) * 0.2f + 0.8f);
		}
		return;
	}
	//-----------------------计算BIAS
	if (inIndicatorName == "BIAS") {
		minBIAS = MAX_FLT, maxBIAS = -MAX_FLT;
		for (auto item : visibleRows) {
			if (item->BIAS0 > maxBIAS) maxBIAS = item->BIAS0;
			if (item->BIAS1 > maxBIAS) maxBIAS = item->BIAS1;
			if (item->BIAS2 > maxBIAS) maxBIAS = item->BIAS2;
			if (item->BIAS0 < minBIAS) minBIAS = item->BIAS0;
			if (item->BIAS1 < minBIAS) minBIAS = item->BIAS1;
			if (item->BIAS2 < minBIAS) minBIAS = item->BIAS2;
		}
		BroadcastIndicatorValueRangeByIndicatorName(indicatorName);
		float BIASValueRange = maxBIAS - minBIAS;
		biasOutLines.X = ((1.f - (70.0f - minBIAS) / BIASValueRange) * 0.2f + 0.8f);
		biasOutLines.Y = ((1.f - (30.0f - minBIAS) / BIASValueRange) * 0.2f + 0.8f);
		BIASUnitPoints0.Empty();
		BIASUnitPoints1.Empty();
		BIASUnitPoints2.Empty();
		BIASUnitPoints0.Reserve(visibleRows.Num());
		BIASUnitPoints1.Reserve(visibleRows.Num());
		BIASUnitPoints2.Reserve(visibleRows.Num());
		for (int i = 0; i < visibleRows.Num(); ++i) {
			BIASUnitPoints0.Add((1.f - (visibleRows[i]->BIAS0 - minBIAS) / BIASValueRange) * 0.2f + 0.8f);
			BIASUnitPoints1.Add((1.f - (visibleRows[i]->BIAS1 - minBIAS) / BIASValueRange) * 0.2f + 0.8f);
			BIASUnitPoints2.Add((1.f - (visibleRows[i]->BIAS2 - minBIAS) / BIASValueRange) * 0.2f + 0.8f);
		}
		return;
	}
}

void UQuantitativeTradingCanves::SetFirstValues_DMI(TArray<TSharedPtr<FQTStockIndex>>& allRows, const int cycleInfos[3]){
	int cycleN = cycleInfos[0];
	float cycleNF = static_cast<float>(cycleN);
	float trSum = allRows[0]->High - allRows[0]->Low;//第一天的TR直接等于第一天的最高价减去最低价
	float pdiSum = 0.0f, ndiSum = 0.0f;
	for (int i = 1; i < cycleN; ++i) {
		float highLow = allRows[i]->High - allRows[i]->Low;
		float highClose = FMath::Abs(allRows[i]->High - allRows[i - 1]->Close);
		float lowClose = FMath::Abs(allRows[i]->Low - allRows[i - 1]->Close);
		trSum += FMath::Max3(highLow, highClose, lowClose);
		float upMove = allRows[i]->High - allRows[i - 1]->High;
		float downMove = allRows[i - 1]->Low - allRows[i]->Low;
		if(upMove> downMove && upMove>0)pdiSum+= upMove;
		if (downMove > upMove && downMove > 0)ndiSum += downMove;
	}
	allRows[cycleN - 1]->TR_Average = trSum / cycleNF;
	allRows[cycleN - 1]->PDI_Average = pdiSum / cycleNF;
	allRows[cycleN - 1]->NDI_Average = ndiSum / cycleNF;
	allRows[cycleN - 1]->PDI = (allRows[cycleN - 1]->PDI_Average / allRows[cycleN - 1]->TR_Average) * 100.0f;
	allRows[cycleN - 1]->NDI = (allRows[cycleN - 1]->NDI_Average / allRows[cycleN - 1]->TR_Average) * 100.0f;
	//计算DX
	allRows[cycleN - 1]->DX = (FMath::Abs(allRows[cycleN - 1]->PDI - allRows[cycleN - 1]->NDI) / (allRows[cycleN - 1]->PDI + allRows[cycleN - 1]->NDI)) * 100.0f;
	//ADX和ADXR的初始值要在2*cycleN-2天才能计算出来,在这里ADX和ADXR都是没有意义的.
}

void UQuantitativeTradingCanves::CalculateAndStoreDMI(TArray<TSharedPtr<FQTStockIndex>>& allRows, int i, const int cycleInfos[3]){
	int cycleN = cycleInfos[0];
	float cycleNF = static_cast<float>(cycleN);
	//计算TR
	float highLow = allRows[i]->High - allRows[i]->Low;
	float highClose = FMath::Abs(allRows[i]->High - allRows[i - 1]->Close);
	float lowClose = FMath::Abs(allRows[i]->Low - allRows[i - 1]->Close);
	float TR_temp = FMath::Max3(highLow, highClose, lowClose);
	//计算TR_Average
	allRows[i]->TR_Average = (allRows[i - 1]->TR_Average * (cycleNF - 1.0f) + TR_temp) / cycleNF;
	//计算PDI_Average和NDI_Average
	float upMove = allRows[i]->High - allRows[i - 1]->High;
	float downMove = allRows[i - 1]->Low - allRows[i]->Low;
	float pdiValue = 0.0f, ndiValue = 0.0f;
	if (upMove > downMove && upMove > 0)pdiValue = upMove;
	if (downMove > upMove && downMove > 0)ndiValue = downMove;
	allRows[i]->PDI_Average = (allRows[i - 1]->PDI_Average * (cycleNF - 1.0f) + pdiValue) / cycleNF;
	allRows[i]->NDI_Average = (allRows[i - 1]->NDI_Average * (cycleNF - 1.0f) + ndiValue) / cycleNF;
	//计算PDI和NDI
	allRows[i]->PDI = (allRows[i]->PDI_Average / allRows[i]->TR_Average) * 100.0f;
	allRows[i]->NDI = (allRows[i]->NDI_Average / allRows[i]->TR_Average) * 100.0f;
	//计算DX
	allRows[i]->DX = (FMath::Abs(allRows[i]->PDI - allRows[i]->NDI) / (allRows[i]->PDI + allRows[i]->NDI)) * 100.0f;
	//计算第一个ADX
	if (i == 2 * cycleNF - 2) {
		float tempDXSum = 0.0f;
		for (int j = cycleN - 1; j <= i; ++j) {
			tempDXSum += allRows[j]->DX;
		}
		allRows[i]->ADX = tempDXSum / cycleNF;
	}
	//计算ADX
	if (i > 2 * cycleNF - 2)	allRows[i]->ADX = (allRows[i - 1]->ADX * (cycleNF - 1.0f) + allRows[i]->DX) / cycleNF;
	//计算ADXR
	int cycleM = cycleInfos[1];
	float cycleMF = static_cast<float>(cycleM);
	if (i > 2 * cycleN + cycleM - 4) {
		allRows[i]->ADXR = (allRows[i]->ADX + allRows[i - cycleM + 1]->ADX) * 0.5f;
	}
}

void UQuantitativeTradingCanves::SetFirstValues_CCI(TArray<TSharedPtr<FQTStockIndex>>& allRows, const int cycleInfos[3]){
	int cycleN = cycleInfos[0];
	float cycleNF = static_cast<float>(cycleN);
	float tpSum = 0.0f;
	for (int i = 0; i < cycleN; ++i) {
		allRows[i]->CCI_TP = (allRows[i]->High + allRows[i]->Low + allRows[i]->Close) / 3.0f;
		tpSum += allRows[i]->CCI_TP;
	}
	allRows[cycleN - 1]->CCI_TPSUM = tpSum;
	allRows[cycleN - 1]->CCI_SMA = tpSum / cycleNF;
	//计算绝对平均偏差
	float absDevSum = 0.0f;
	for (int i = 0; i < cycleN; ++i) {
		absDevSum += FMath::Abs(allRows[i]->CCI_TP - allRows[cycleN - 1]->CCI_SMA);
	}
	allRows[cycleN - 1]->CCI_MAD = absDevSum / cycleNF;
	//计算CCI
	allRows[cycleN - 1]->CCI = (allRows[cycleN - 1]->CCI_TP - allRows[cycleN - 1]->CCI_SMA) / (0.015f * allRows[cycleN - 1]->CCI_MAD);
}

void UQuantitativeTradingCanves::CalculateAndStoreCCI(TArray<TSharedPtr<FQTStockIndex>>& allRows, int i, const int cycleInfos[3]){
	int cycleN = cycleInfos[0];
	float cycleNF = static_cast<float>(cycleN);
	//计算CCI_TP
	allRows[i]->CCI_TP = (allRows[i]->High + allRows[i]->Low + allRows[i]->Close) / 3.0f;
	//计算CCI_TPSUM和CCI_SMA
	allRows[i]->CCI_TPSUM = allRows[i - 1]->CCI_TPSUM - allRows[i - cycleN]->CCI_TP + allRows[i]->CCI_TP;
	allRows[i]->CCI_SMA = allRows[i]->CCI_TPSUM / cycleNF;
	//计算绝对平均偏差CCI_MAD
	float absDevSum = 0.0f;
	for (int j = 0; j < cycleN; ++j) {
		absDevSum += FMath::Abs(allRows[i - j]->CCI_TP - allRows[i]->CCI_SMA);
	}
	allRows[i]->CCI_MAD = absDevSum / cycleNF;
	//计算CCI
	allRows[i]->CCI = (allRows[i]->CCI_TP - allRows[i]->CCI_SMA) / (0.015f * allRows[i]->CCI_MAD);
}

void UQuantitativeTradingCanves::SetFirstValues_BIAS(TArray<TSharedPtr<FQTStockIndex>>& allRows, const int cycleInfos[3],int crv){
	int cycleN = cycleInfos[0];
	float cycleNF = static_cast<float>(cycleN);
	float smaSum = 0.0f;
	for (int i = 0; i < cycleN; ++i) {
		smaSum += allRows[i]->Close;
	}
	float sma = smaSum / cycleNF;
	if (crv == 0) {
		allRows[cycleN - 1]->BIAS0_SMASUM = smaSum;
		allRows[cycleN - 1]->BIAS0 = (allRows[cycleN - 1]->Close - sma) / sma * 100.0f;
	}
	else if (crv == 1) {
		allRows[cycleN - 1]->BIAS1_SMASUM = smaSum;
		allRows[cycleN - 1]->BIAS1 = (allRows[cycleN - 1]->Close - sma) / sma * 100.0f;
	}
	else if (crv == 2) {
		allRows[cycleN - 1]->BIAS2_SMASUM = smaSum;
		allRows[cycleN - 1]->BIAS2 = (allRows[cycleN - 1]->Close - sma) / sma * 100.0f;
	}
}

void UQuantitativeTradingCanves::CalculateAndStoreBIAS(TArray<TSharedPtr<FQTStockIndex>>& allRows, int i, const int cycleInfos[3],int crv){
	int cycleN = cycleInfos[0];
	float cycleNF = static_cast<float>(cycleN);
	//计算BIAS0
	if(crv==0) {
		allRows[i]->BIAS0_SMASUM = allRows[i - 1]->BIAS0_SMASUM - allRows[i - cycleN]->Close + allRows[i]->Close;
		float sma0 = allRows[i]->BIAS0_SMASUM / cycleNF;
		allRows[i]->BIAS0 = (allRows[i]->Close - sma0) / sma0 * 100.0f;
	}
	//计算BIAS1
	else if (crv == 1) {
		allRows[i]->BIAS1_SMASUM = allRows[i - 1]->BIAS1_SMASUM - allRows[i - cycleN]->Close + allRows[i]->Close;
		float sma1 = allRows[i]->BIAS1_SMASUM / cycleNF;
		allRows[i]->BIAS1 = (allRows[i]->Close - sma1) / sma1 * 100.0f;
	}
	//计算BIAS2
	else if (crv == 2) {
		allRows[i]->BIAS2_SMASUM = allRows[i - 1]->BIAS2_SMASUM - allRows[i - cycleN]->Close + allRows[i]->Close;
		float sma2 = allRows[i]->BIAS2_SMASUM / cycleNF;
		allRows[i]->BIAS2 = (allRows[i]->Close - sma2) / sma2 * 100.0f;
	}
}

void UQuantitativeTradingCanves::BroadcastIndicatorValueRangeByIndicatorName(FName inIndicatorName){
	if (inIndicatorName == "Volume")IndicatorValueChangeDelegate.Broadcast(FString::Printf(TEXT("%.2f"), maxVolume), FString::Printf(TEXT("%.2f"), minVolume), "");
	else if (inIndicatorName == "MACD")IndicatorValueChangeDelegate.Broadcast(FString::Printf(TEXT("%.2f"), absmaxMACD), FString::Printf(TEXT("%.2f"), -absmaxMACD), "0.00");
	else if (inIndicatorName == "KDJ")IndicatorValueChangeDelegate.Broadcast(FString::Printf(TEXT("%.2f"), maxKDJ), FString::Printf(TEXT("%.2f"), minKDJ), FString::Printf(TEXT("%.2f"), (maxKDJ + minKDJ) * 0.5f));
	else if (inIndicatorName == "RSI")IndicatorValueChangeDelegate.Broadcast(FString::Printf(TEXT("%.2f"), maxRSI), FString::Printf(TEXT("%.2f"), minRSI), FString::Printf(TEXT("%.2f"), (maxRSI + minRSI) * 0.5f));
	else if (inIndicatorName == "WR")IndicatorValueChangeDelegate.Broadcast(FString::Printf(TEXT("%.2f"), maxWR), FString::Printf(TEXT("%.2f"), minWR), FString::Printf(TEXT("%.2f"), (maxWR + minWR) * 0.5f));
	else if (inIndicatorName == "DMI")IndicatorValueChangeDelegate.Broadcast(FString::Printf(TEXT("%.2f"), maxDMI), FString::Printf(TEXT("%.2f"), minDMI), FString::Printf(TEXT("%.2f"), (maxDMI + minDMI) * 0.5f));
	else if (inIndicatorName == "CCI")IndicatorValueChangeDelegate.Broadcast(FString::Printf(TEXT("%.2f"), maxCCI), FString::Printf(TEXT("%.2f"), minCCI), FString::Printf(TEXT("%.2f"), (maxCCI + minCCI) * 0.5f));
	else if (inIndicatorName == "BIAS")IndicatorValueChangeDelegate.Broadcast(FString::Printf(TEXT("%.2f"), maxBIAS), FString::Printf(TEXT("%.2f"), minBIAS), FString::Printf(TEXT("%.2f"), (maxBIAS + minBIAS) * 0.5f));
}

void UQuantitativeTradingCanves::OnCompanyCommitted(const TArray<TSharedPtr<FQTStockIndex>>& inAllRows){
	allStockIndexRows = inAllRows;
	//根据startDate和currentDate截取数据
	if (!allStockIndexRows.IsEmpty()) {
		//计算并存储各种技术指标
		CaculateAndStoreIndicators(allStockIndexRows);
		currentDate = allStockIndexRows.Last()->Date;//最新日期
		if (startDate == 0)startDate = currentDate - 10000;//如果startDate没有被设置过,则默认显示1年的数据
		GetIntervalRow(allStockIndexRows, visibleRows, startDate, currentDate);
	}
	sampledPoints = SampleDataFromDataTable();
	visibleCounts = visibleRows.Num();
}
