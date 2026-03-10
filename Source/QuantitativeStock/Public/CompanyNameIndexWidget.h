// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UObject/NoExportTypes.h"
#include "Http.h"
#include "CompanyNameIndexWidget.generated.h"

struct FQTStockListRow;
struct FQTCompanyAbstractRow;
struct FQTStockIndex;//日线数据结构体
struct FQTStockRealTimeData;//实时行情数据结构

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnIntroductionUpdate, const FQTCompanyAbstractRow&, abstractData);

UCLASS()
class QUANTITATIVESTOCK_API UCompanyNameIndexWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "QT | Assets")
	UDataTable* stockListDataTable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QT | Widgets")
	class UQuantitativeTradingCanves* mainCanvas;
	UPROPERTY(EditAnywhere, Category = "QT | Widgets")
	class UStockListDownWidget* stockListDownWidgetBP;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QT | Widgets")
	class URegexMatchedWidget* RegexMatchedWidgetBP;
	//通过股票代码或公司名称获取对应的介绍信息(公司的介绍信息存储在DataTable中)
	UFUNCTION(BlueprintCallable, Category = "QT | Functions")
	void GetIntroductionByCodeOrName(FString stockCodeOrCompanyName);
	//获取K线数据的蓝图可调用函数
	UFUNCTION(BlueprintCallable, Category = "QT | Functions")
	void GetKLineDatasBP(const FString& stockCode, int inklt = 101, int  infqt = 1);
	//保存股票数据到指定下拉列表中
	UFUNCTION(BlueprintCallable, Category = "QT | Functions")
	void SaveDownListStocksBP(const FString& codeOrName, const FString& filename);
	//保存最近访问的股票数据到指定路径(每次点击搜索按钮都会存储到最近访问列表)
	UFUNCTION(BlueprintCallable, Category = "QT | Functions")
	void SaveToRecentListPathBP(const FString& codeOrName);
	//保存下拉列表数据到指定路径(每次点击金色按钮都会存储上一个按钮下来菜单下的最新数据)
	UFUNCTION(BlueprintCallable, Category = "QT | Functions")
	void SavePreStockDownListDatasFromDownListWidget(const FString& savedFile);
	//获取股票列表最新实时数据
	UFUNCTION(BlueprintCallable, Category = "QT | Functions")
	void GetStockDownListDatas(const FString& savedFile);
	//通过正则匹配获取股票或基金列表数据
	UFUNCTION(BlueprintCallable, Category = "QT | Functions")
	void RegexListDatas(const FString& nameorcode);
	//更新公司简介广播代理
	UPROPERTY(BlueprintAssignable,Category = "QT | Delegate")
	FOnIntroductionUpdate onIntroductionUpdate;
	//根据名称或代码获取证券的检索信息(0代表股票信息,1代表基金信息,默认股票)
	TSharedPtr<FQTStockListRow> GetFQTStockListRowByCodeOrName(FString innameorcode, int16 stockOrFund = 0);
	UFUNCTION(BlueprintImplementableEvent, Category = "QT | Events")
	void OnStockCodeTextCommit(const FString& stockcode);

	//更新outKLineDatas_里的最后一天的数据为最新的实时数据(当K线数据的最后一天日期与最新实时数据的日期相同的时候更新最后一天的数据,否则不更新)
	void UpdateLatestDayLine(const FQTStockRealTimeData& latestDayLineData);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	//监控实时数据
	class UStockMonitor* stockMonitor_;
	//HTTP响应处理(默认腾讯网的数据)
	void HandleStockDataResponse(const FString& ResponseData, int32 insource = 1);
	void HandleF10DataResponse(const FString& ResponseData, int32 insource = 0);
	void HandleStockDataError(int32 ErrorCode, const FString& ErrorMessage);
	//下拉列表股票最新数据
	TArray<FQTStockRealTimeData> listStocksDatas_;

	//存储股票代码或公司名称对应的股票列表检索信息数据
	TMap<FString, TSharedPtr<FQTStockListRow>> StockRowListMap_;
	//存储基金代码或基金名称对应的基金列表检索信息数据
	TMap<FString, TSharedPtr<FQTStockListRow>> FundRowListMap_;
	//存储最近访问的股票
	TArray<TSharedPtr<FQTStockListRow>> DownStockList_;
	//当前代码公司名称对应的文件名路径 | 文件格式: 公司简称+股票代码,例如: 贵州茅台600519
	FString currentFilename_;
	//存储获取到的最新K线数据
	TArray<TSharedPtr<FQTStockIndex>> outKLineDatas_;
	//通过股票代码获取对应的日线数据(默认所有前复权数据 fqt复权类型: 0代表不复权 1代表前复权 2代表后复权 ; klt周期类型)
	/*
	klt周期
	日线 101
	周线 102
	月线 103
	5分钟 5
	15分钟 15
	30分钟 30
	60分钟 60
	*/
	bool GetKLineDatasByStockCode(const FString& stockCode, int inklt = 101, int  infqt = 1);
	//生成URL
	FString GetKLineDataURL(const FString& StockCode, int inklt = 101, int  infqt = 1);
	//解析返回的K线数据
	bool ParseKLineDataResponse(const FString& ResponseString, TArray<TSharedPtr<FQTStockIndex>>& outKLineDatas);
	//解析返回的股票列表数据
	bool ParseStockListDataResponse(const FString& ResponseString, TMap<FString, TSharedPtr<FQTStockListRow>>& outStockListMap);
	//解析来自东方财富网的基金列表数据
	bool ParseFundListDataResponse(const FString& ResponseString, TMap<FString, TSharedPtr<FQTStockListRow>>& outFundListMap);
	//处理K线数据HTTP请求完成
	void OnKLineDataRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	//处理股票列表数据HTTP请求完成
	void OnStockListDataRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	//处理基金列表数据HTTP请求完成
	void OnFundListDataRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	//处理公司介绍数据HTTP请求完成
	void OnIntroductionDataRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	//发送HTTP请求获取K线数据
	void FetchKLineData(const FString& StockCode, int inklt = 101, int  infqt = 1);
	//发送HTTP请求获取股票列表数据
	void FetchStockListData();
	//发送HTTP请求获取基金列表数据
	void FetchFundListData();
	//发送HTTP请求获取公司介绍数据
	void FetchCompanyIntroductionData(const FString& codeOrName);
	//在程序开始的时候获取最近访问的历史数据
	void GetRecentStockList(const FString& filename);
	//在程序结束的时候保存最近访问的历史数据
	void SaveRecentStockList(const FString& filename);
};
