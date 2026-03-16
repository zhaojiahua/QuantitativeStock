// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Overlay.h"
#include "QuantitativeTradingCanves.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnValueChange, FString, newMaxValue, FString, newMinVallue, FString, newZeroValue);

class UCurveVector;
class UQTTreeViewItemObj;
struct FQTStockIndex;
UCLASS()
class QUANTITATIVESTOCK_API UQuantitativeTradingCanves : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "QT | Assets")
	UCurveVector* selfVectorCrv;
	UPROPERTY(EditDefaultsOnly, Category = "QT | Params")
	int samplingCounts = 200;//采样密度默认是200个点,这个采样点个数会直接影响绘制图形的平滑度和精确性
	UPROPERTY(EditDefaultsOnly, Category = "QT | Params")
	TSubclassOf<UUserWidget> KLineFlotWindWidgetClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "QT | Params")
	class UQuantitativeTradingWidget* quantitativeTradingWidget;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "QT | Params")
	class UCompanyNameIndexWidget* companyNameIndexWidget;
	UPROPERTY(meta = (BindWidget))
	UOverlay* overlayForFloatWind;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "QT | Indicator")
	FName indicatorName = "Volume";
	UPROPERTY(BlueprintAssignable, Category = "QT | Indicator")
	FOnValueChange IndicatorValueChangeDelegate;
	UFUNCTION(BlueprintCallable, Category = "QT | Indicator | Setting")
	void ReDrawSpecifyIndicator(FString inSpecifyName, FVector3f incycles);
	UFUNCTION(BlueprintCallable, Category = "QT | Indicator | Setting")
	void SetSpecifyIndicatorShow(FString inSpecifyName, bool showornot);
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "QT | Indicator")
	float minVolume = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "QT | Indicator")
	float maxVolume = 0.0f;
	float minKDJ = MAX_FLT, maxKDJ = -MAX_FLT;//存放KDJ指标的最小值和最大值
	float minRSI = MAX_FLT, maxRSI = -MAX_FLT;//存放RSI指标的最小值和最大值
	float minWR = MAX_FLT, maxWR = 0.0f;//存放WR指标的最小值和最大值
	float minDMI = 100.0f, maxDMI = 0.0f;//存放DMI指标的最小值和最大值
	float minCCI = MAX_FLT, maxCCI = -MAX_FLT;//存放CCI指标的最小值和最大值
	float minBIAS = MAX_FLT, maxBIAS = -MAX_FLT;//存放BIAS指标的最小值和最大值
	float absmaxMACD = 0.0f;//存放MACD的绝对值最大值
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite, Category = "QT | SMA")
	bool sma5 = false;
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite, Category = "QT | SMA")
	bool sma10 = false;
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite, Category = "QT | SMA")
	bool sma20 = false;
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite, Category = "QT | SMA")
	bool sma60 = false;
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite, Category = "QT | SMA")
	bool sma240 = false;
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite, Category = "QT | EMA")
	bool ema5 = false;
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite, Category = "QT | EMA")
	bool ema10 = false;
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite, Category = "QT | EMA")
	bool ema20 = false;
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite, Category = "QT | EMA")
	bool ema60 = false;
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite, Category = "QT | EMA")
	bool ema240 = false;
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite, Category = "QT | Indicator")
	bool boll = false;
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite, Category = "QT | Indicator")
	bool kline = true;
	UFUNCTION(BlueprintCallable, Category = "QT")
	void SetMAVisible(UQTTreeViewItemObj* inItemObj);
	UFUNCTION(BlueprintCallable, Category = "QT")
	void InitialCanvas();
	//根据ComboBoxString的ItemString截取不同时段的数据
	UFUNCTION(BlueprintCallable, Category = "QT")
	void GetIntervalRowByStringItem(int inItemIndex);
	UFUNCTION(BlueprintCallable, Category = "QT")
	void OnIndicatorItemChanged(FName inIndicatorName);
	//更新最新的日线数据,并且根据最新的日线数据重新计算技术指标,最后重新绘制图表
	UFUNCTION(BlueprintCallable, Category = "QT")
	void UpdateLatestDayLine(FQTStockRealTimeData inRealTimeData);
	//从json文件里读取周期参数（蓝图可调用）
	UFUNCTION(BlueprintCallable, Category = "QT")
	void LoadCycleSettingsFromJson_BP(const FString& inIndicatorName, int& out1, int& out2, int& out3);

	//从inVectorCrv[dimension]曲线上均匀采样dataCounts个点,然后绘制曲线在AllottedGeometry上,并且三根曲线的取值范围会作为一个整体缩放到适配AllottedGeometry的大小.
	TArray<FVector2f>SampleDataFromCurve(UCurveVector* inVectorCrv, const FGeometry& AllottedGeometry, int dimension = 0)const;

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual void NativePreConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)override;

private:
	//绘制虚线,space是虚线间隔所占虚线总长度的百分比
	void DrawDottedLine(const FPaintContext& paintContext, FVector2f startPosition, FVector2f endPosition, FLinearColor lineColor, float space = 1.f)const;
	//截取指定日期区间的数据
	void GetIntervalRow(const TArray<TSharedPtr<FQTStockIndex>>& inalldatas, TArray<TSharedPtr<FQTStockIndex>>& outVisibleRows, int cutStart, int cutEnd);
	//从DataTable里面读取相应的数据
	TArray<FVector2f>SampleDataFromDataTable();
	//计算并存储各种技术指标
	void CaculateAndStoreIndicators(TArray<TSharedPtr<FQTStockIndex>>& allRows);
	//存储设置的周期参数到json文件里
	bool SaveCycleSettingsToJson(const FString& inSpecifyName, const int cycleInfos[3]);
	//从json文件里读取周期参数
	bool LoadCycleSettingsFromJson(const FString& inIndicatorName, int cycleInfos[3]);
	//重新计算并绘制指定的指标
	void ReCaculateSpecifyIndicator(FString inSpecifyName, const int cycleInfos[3]);
	//重新计算并存储最新日期的各种技术指标
	void ReCaculateAndStoreLatestDayKLine(const FQTStockRealTimeData& inRealTimeData);
	void RefreshVisibleRows();
	void ReSampleSpecifyIndicator(FString inSpecifyName);

	void SetFirstValues_DMI(TArray<TSharedPtr<FQTStockIndex>>& allRows, const int cycleInfos[3]);
	void CalculateAndStoreDMI(TArray<TSharedPtr<FQTStockIndex>>& allRows, int i, const int cycleInfos[3]);
	void SetFirstValues_CCI(TArray<TSharedPtr<FQTStockIndex>>& allRows, const int cycleInfos[3]);
	void CalculateAndStoreCCI(TArray<TSharedPtr<FQTStockIndex>>& allRows, int i, const int cycleInfos[3]);
	void SetFirstValues_BIAS(TArray<TSharedPtr<FQTStockIndex>>& allRows, const int cycleInfos[3], int crv);
	void CalculateAndStoreBIAS(TArray<TSharedPtr<FQTStockIndex>>& allRows, int i, const int cycleInfos[3], int crv);

	TArray<FVector2f> sampledPoints;
	TArray<float> sma5UnitPoints;
	TArray<float> sma10UnitPoints;
	TArray<float> sma20UnitPoints;
	TArray<float> sma60UnitPoints;
	TArray<float> sma240UnitPoints;
	TArray<float> ema5UnitPoints;
	TArray<float> ema10UnitPoints;
	TArray<float> ema20UnitPoints;
	TArray<float> ema60UnitPoints;
	TArray<float> ema240UnitPoints;

	TArray<float> volumeUnitPoints;//成交量归一化后的结果
	TArray<FVector3f> MACDUnitPoints;//MACD的归一化后的结果(x存放DIF,y存放DEA,z存放MACD值)
	TArray<FVector2f> BollUnitPoints;//存放Boll线上下轨的归一化结果(x存放上轨,y存放下轨)
	TArray<FVector3f> KDJUnitPoints;//存放KSJ指标的归一化结果(x存放K值,y存放D值,z存放J值)
	TArray<float> RSIUnitPoints0;//存放RSI0指标的归一化结果
	TArray<float> RSIUnitPoints1;//存放RSI1指标的归一化结果
	TArray<float> RSIUnitPoints2;//存放RSI2指标的归一化结果
	TArray<float> WRUnitPoints1;//存放RSI1指标的归一化结果
	TArray<float> WRUnitPoints2;//存放RSI2指标的归一化结果
	TArray<FVector4f> DMIUnitPoints;//存放DMI指标的归一化结果(x存放PDI,y存放NDI,z存放ADX,w存放ADXR)
	TArray<float> CCIUnitPoints;//存放CCI指标的归一化结果
	TArray<float> BIASUnitPoints0;//存放BIAS0指标的归一化结果
	TArray<float> BIASUnitPoints1;//存放BIAS1指标的归一化结果
	TArray<float> BIASUnitPoints2;//存放BIAS2指标的归一化结果
	FVector4f kdjOutLines;//存放KDJ指标的判定线
	FVector2f rsiOutLines;//存放RSI指标的判定线
	FVector2f wrOutLines;//存放WR指标的判定线
	FVector2f dmiOutLines;//存放DMI指标的判定线
	FVector2f cciOutLines;//存放CCI指标的判定线
	FVector2f biasOutLines;//存放BIAS指标的判定线
	bool rsi0show = true;
	bool rsi1show = true;
	bool rsi2show = true;
	bool bias0show = true;
	bool bias1show = true;
	bool bias2show = true;

	TArray<FVector2f> KLineUnitPoints1;//存放K线的开盘价和收盘价,大的价在前,小的价在后
	TArray<FLinearColor> KLineUnitColors1;//存放K线的颜色,与KLineUnitPoints1一一对应
	TArray<FVector2f> KLineUnitPoints2;//存放K线的最高价和最低价,最高价在前,最低价在后
	TArray<UUserWidget*> KLineFlotWindWidgets;//存放K线浮动窗口

	int allCounts = 0;//存储下数据总数目
	int startDate = 0;//存储当前显示数据的起始日期-获取最新数据时确定
	int currentDate = 0;//存储当前显示数据的结束日期-获取最新数据时确定
	int visibleCounts = 0;//屏幕中间可见的数目-鼠标动态控制
	int leftOutCounts = 0;//超出屏幕左边的累计数目-鼠标动态控制
	int rightOutCounts = 0;//超出屏幕右边的累计数目-鼠标动态控制

	bool startMove = false;//开始检测鼠标移动
	FDeprecateSlateVector2D preMousePos;//鼠标指针上一帧的位置

	TArray<TSharedPtr<FQTStockIndex>> allStockIndexRows;//存放所有行数据
	TArray<TSharedPtr<FQTStockIndex>> visibleRows;//存放当前可见的行数据
	//根据IndicatorName广播不同的指标最大最小值
	void BroadcastIndicatorValueRangeByIndicatorName(FName inIndicatorName);

public:
	//响应公司信息提交的委托
	void OnCompanyCommitted(const TArray<TSharedPtr<FQTStockIndex>>& inAllRows);
};
