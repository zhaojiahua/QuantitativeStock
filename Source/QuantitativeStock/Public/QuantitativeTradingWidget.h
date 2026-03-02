// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QTCurveVectorActor.h"
#include <Components/TreeView.h>
#include "QuantitativeTradingWidget.generated.h"


// 委托：股票实时行情数据更新
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStockRealTimeDataUpdated, const FQTStockRealTimeData&, StockRealTimeData);
//委托: F10财务数据更新
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnF10DataUpdated, const FQTFinancialF10Main&, F10Data);
// 委托：监控错误
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStockMonitorError, const FString&, StockCode, const FString&, ErrorMessage);

struct FQTFinancialF10Main;
UCLASS()
class QUANTITATIVESTOCK_API UQuantitativeTradingWidget : public UUserWidget
{
	GENERATED_BODY()
	
	UFUNCTION(BlueprintCallable, BlueprintPure ,Category = "QT")
	UQTTreeViewItemObj* BuildUQTTreeViewItemObj(FString inname, int indepth);
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "QT")
	int GetIndexFromItemString(FString inString);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

public:
	//获取当前股票的最新财务数据(直接从网站获取最新数据,包括各种财务指标和资产负债表利润表的摘要数据,0代表使用的是东方财富网站API)
	UFUNCTION(BlueprintCallable, Category = "QT")
	void GetLatestF10FinanceData(const FString& StockCode, int32 insource = 0);

	UFUNCTION(BlueprintImplementableEvent, Category = "QT")
	void ClearScreen();

	// 开始监控单只股票(传入股票代码,刷新时间间隔,数据来源)
	UFUNCTION(BlueprintCallable, Category = "QT | StockMonitor")
	void StartMonitoring(const FString& StockCode, float IntervalSeconds = 5.0f, int32 insource = 0);
	// 停止监控单只股票
	UFUNCTION(BlueprintCallable, Category = "QT | StockMonitor")
	void StopMonitoring();
	//批量开始监控多只股票
	UFUNCTION(BlueprintCallable, Category = "QT | StockMonitor")
	void StartMonitoringMultiple(const TArray<FString>& StockCodes, float IntervalSeconds = 10.0f);
	//获取最新股票数据
	UFUNCTION(BlueprintCallable, BlueprintPure,Category = "QT | StockMonitor")
	FQTStockRealTimeData GetLatestStockData() const;
	//获取所有监控股票的最新数据
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "QT | StockMonitor")
	TArray<FQTStockRealTimeData> GetAllLatestStockData() const;

	//事件：股票数据更新
	UPROPERTY(BlueprintAssignable, Category = "QT | StockMonitor")
	FOnStockRealTimeDataUpdated onStockRealTimeDataUpdated;
	//事件：F10数据更新
	UPROPERTY(BlueprintAssignable, Category = "QT | StockMonitor")
	FOnF10DataUpdated onF10Updated;
	//事件：监控错误
	UPROPERTY(BlueprintAssignable, Category = "QT | StockMonitor")
	FOnStockMonitorError onStockMonitorError;

private:
	//定时更新股票数据
	void UpdateStockData();
	//HTTP响应处理
	void HandleStockDataResponse(const FString& ResponseData, int32 insource = 0);
	void HandleF10DataResponse(const FString& ResponseData, int32 insource = 0);
	void HandleStockDataError(int32 ErrorCode, const FString& ErrorMessage);
	//存放最新F10财务数据
	FQTFinancialF10Main f10Data;


	//股票监控器对象
	UPROPERTY()
	class UStockMonitor* stockMonitor_;
	// 当前监控的股票代码
	FString currentStockCode_;
	// 所有监控的股票代码
	TArray<FString> allStockCodes_;
	// 监控间隔（秒）
	float monitorInterval_;
	//定时器句柄
	FTimerHandle monitorTimerHandle_;
	//是否正在监控
	bool isMonitoring_ = false;
	//数据来源
	int32 insource_ = 0;
	//最新股票数据
	TMap<FString, FQTStockRealTimeData> latestStockDataMap_;
	//历史股票数据
	TMap<FString, TArray<FQTStockRealTimeData>> historicalStockDataMap_;
};

UCLASS(Blueprintable)
class UQTTreeViewItemObj : public UObject
{
	GENERATED_BODY()

public:
	UQTTreeViewItemObj() {}

	//均线名称
	UPROPERTY(BlueprintReadWrite, Category = "QT")
	FString itemName;
	//ItemWidget引用
	UPROPERTY(BlueprintReadWrite, Category = "QT")
	UUserWidget* itemWidget;
	UPROPERTY(BlueprintReadWrite, Category = "QT")
	bool isChecked;
	UPROPERTY(BlueprintReadWrite, Category = "QT")
	int depth = 0;
	UPROPERTY(BlueprintReadWrite, Category = "QT")
	TArray<UQTTreeViewItemObj*> subItems;

	//添加Item
	UFUNCTION(BlueprintCallable ,Category = "QT")
	void AddSubItem(UQTTreeViewItemObj* subItem);
};