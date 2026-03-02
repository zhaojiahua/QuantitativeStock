// Fill out your copyright notice in the Description page of Project Settings.


#include "QTCurveVectorActor.h"
#include <Curves/CurveVector.h>

// Sets default values
AQTCurveVectorActor::AQTCurveVectorActor(){
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AQTCurveVectorActor::BeginPlay()
{
	Super::BeginPlay();
	//存储原始曲线数据
	orgCrv0 = vectorCrv->FloatCurves[0];
	orgCrv1 = vectorCrv->FloatCurves[1];
	orgCrv2 = vectorCrv->FloatCurves[2];
	if(orgCrv0.GetNumKeys()>0)	orgTimeRange0 = orgCrv0.GetLastKey().Time - orgCrv0.GetFirstKey().Time;
	if(orgCrv1.GetNumKeys()>0) orgTimeRange1 = orgCrv1.GetLastKey().Time - orgCrv1.GetFirstKey().Time;
	if(orgCrv2.GetNumKeys()>0) orgTimeRange2 = orgCrv2.GetLastKey().Time - orgCrv2.GetFirstKey().Time;
	orgTimeStep0 = orgTimeRange0 / samplingCounts;
	orgTimeStep1 = orgTimeRange1 / samplingCounts;
	orgTimeStep2 = orgTimeRange2 / samplingCounts;
}

// Called every frame
void AQTCurveVectorActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (startConvolution) {
		timeDuration += DeltaTime;
		if (timeDuration >= 5.0f / samplingCounts) {//5秒钟移动完
			FRichCurve& fcrv0 = vectorCrv->FloatCurves[0];
			FRichCurve& fcrv1 = vectorCrv->FloatCurves[1];
			FRichCurve& fcrv2 = vectorCrv->FloatCurves[2];
			//卷积计算,把计算结果实时地添加到z曲线上
			FKeyHandle fcrv2TempKeyHandle = fcrv2.UpdateOrAddKey(fcrv0.GetLastKey().Time, FixedConvolution(fcrv0, fcrv1));
			fcrv2.SetKeyInterpMode(fcrv2TempKeyHandle, ERichCurveInterpMode::RCIM_Linear);
			fcrv2.SetKeyTangentMode(fcrv2TempKeyHandle, ERichCurveTangentMode::RCTM_Break);
			
			//x曲线向右移动
			FKeyHandle tempkeyH = fcrv0.GetFirstKeyHandle();
			while (tempkeyH) {
				fcrv0.GetKey(tempkeyH).Time += orgTimeStep1;
				tempkeyH = fcrv0.GetNextKey(tempkeyH);
			}
			if (fcrv0.GetLastKey().Time> fcrv1.GetLastKey().Time) { startConvolution = false; }//当x曲线的右边界跟y曲线的右边界对齐时停止
			
			timeDuration = 0.0f;
		}
	}

}

void AQTCurveVectorActor::StartConvolution(){
	if (vectorCrv == nullptr) {
		UE_LOG(LogTemp, Error, TEXT("AQTCurveVectorActor::StartConvolution::vectorCrv is null"));
		return;
	}
	FRichCurve& fcrv0 = vectorCrv->FloatCurves[0];
	FRichCurve& fcrv1 = vectorCrv->FloatCurves[1];
	float mintime0, mintime1, maxtime0, maxtime1;
	fcrv0.GetTimeRange(mintime0, maxtime0);
	fcrv1.GetTimeRange(mintime1, maxtime1);
	float moveleft = mintime1 - maxtime0;//y曲线的左边界到x曲线右边界的距离(代表过去N天的收益率按照不同的权重加和求平均,得到的今天的收益率)

	//将x曲线移动至其中轴线与y曲线的左边界对齐的位置
	FKeyHandle tempkeyH = fcrv0.GetFirstKeyHandle();
	while (tempkeyH) {
		fcrv0.GetKey(tempkeyH).Time += moveleft;
		tempkeyH = fcrv0.GetNextKey(tempkeyH);
	}
	convolutionResult.Empty();//开始卷积之前把盛放结果的数组清空一下
	startConvolution = true;
}

float AQTCurveVectorActor::FixedConvolution(FRichCurve crv0, FRichCurve crv1)
{
	float Sum=0.0f;
	//每个曲线都在x轴向上平均采样100个点用来近似模拟函数
	float timeStep = (crv0.GetLastKey().Time - crv0.GetFirstKey().Time) / samplingCounts;
	for (float i = crv0.GetFirstKey().Time; i < crv0.GetLastKey().Time; i += timeStep) {
		Sum += crv0.Eval(i) * crv1.Eval(i);
	}
	return Sum * timeStep;
}

void AQTCurveVectorActor::CaculateAndStoreMAIntoDataTable()
{
	if (dataTable == nullptr)return;
	//计算并存储移动平均线到DataTable里
	TArray<FQTStockIndex*> allRows;
	dataTable->GetAllRows<FQTStockIndex>(TEXT(""), allRows);
	//初始化第一个交易日的MA值
	allRows[0]->SMA5 = allRows[0]->Close;
	allRows[0]->SMA10 = allRows[0]->Close;
	allRows[0]->SMA20 = allRows[0]->Close;
	allRows[0]->SMA60 = allRows[0]->Close;
	allRows[0]->SMA240 = allRows[0]->Close;
	allRows[0]->EMA5 = allRows[0]->Close;
	allRows[0]->EMA10 = allRows[0]->Close;
	allRows[0]->EMA20 = allRows[0]->Close;
	allRows[0]->EMA60 = allRows[0]->Close;
	allRows[0]->EMA240 = allRows[0]->Close;
	//计算后续交易日的MA值
	float alpha5 = 2.0f / (5 + 1);
	float alpha10 = 2.0f / (10 + 1);
	float alpha20 = 2.0f / (20 + 1);
	float alpha60 = 2.0f / (60 + 1);
	float alpha240 = 2.0f / (240 + 1);
	for(int i=1;i<allRows.Num(); i++) {
		allRows[i]->SMA5 = (allRows[i - 1]->SMA5 * 4 + allRows[i]->Close) / 5.0f;
		allRows[i]->SMA10 = (allRows[i - 1]->SMA10 * 9 + allRows[i]->Close) / 10.0f;
		allRows[i]->SMA20 = (allRows[i - 1]->SMA20 * 19 + allRows[i]->Close) / 20.0f;
		allRows[i]->SMA60 = (allRows[i - 1]->SMA60 * 59 + allRows[i]->Close) / 60.0f;
		allRows[i]->SMA240 = (allRows[i - 1]->SMA240 * 239 + allRows[i]->Close) / 240.0f;
		allRows[i]->EMA5 = allRows[i - 1]->EMA5 + alpha5 * (allRows[i]->Close - allRows[i - 1]->EMA5);
		allRows[i]->EMA10 = allRows[i - 1]->EMA10 + alpha10 * (allRows[i]->Close - allRows[i - 1]->EMA10);
		allRows[i]->EMA20 = allRows[i - 1]->EMA20 + alpha20 * (allRows[i]->Close - allRows[i - 1]->EMA20);
		allRows[i]->EMA60 = allRows[i - 1]->EMA60 + alpha60 * (allRows[i]->Close - allRows[i - 1]->EMA60);
		allRows[i]->EMA240 = allRows[i - 1]->EMA240 + alpha240 * (allRows[i]->Close - allRows[i - 1]->EMA240);
	}

}

void AQTCurveVectorActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	vectorCrv->FloatCurves[0] = orgCrv0;
	vectorCrv->FloatCurves[1] = orgCrv1;
	vectorCrv->FloatCurves[2] = orgCrv2;
}