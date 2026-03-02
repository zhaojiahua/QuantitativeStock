// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Http.h"
#include "StockMonitor.generated.h"

// 委托：请求成功
DECLARE_DELEGATE_TwoParams(FOnRequestSuccess, const FString& /*ResponseData*/ , int32 /*指明所使用的数据源*/);
//委托: F10请求成功
DECLARE_DELEGATE_TwoParams(FOnRequestSuccessF10, const FString& /*FinancialF10Data*/, int32 /*指明所使用的数据源*/);
// 委托：请求失败
DECLARE_DELEGATE_TwoParams(FOnRequestFailed, int32 /*ErrorCode*/, const FString& /*ErrorMessage*/);

struct FQTStockRealTimeData;
struct FQTFinancialF10Main;
UCLASS()
class QUANTITATIVESTOCK_API UStockMonitor : public UObject
{
	GENERATED_BODY()
	
public:
	UStockMonitor();
	//获取单只股票F10财务数据(指定数据源,0代表东方财富API,1代表腾讯API,2代表新浪API(新浪API暂时不可用))
	void GetStockF10FianceMainData(const FString& inStockCode, int32 indatasource = 0);
	//获取单只股票实时行情数据(指定数据源,0代表东方财富API,1代表腾讯API,2代表新浪API(新浪API暂时不可用))
	void GetStockData(const FString& inStockCode,int32 indatasource=0);
	//获取多只股票实时行情数据(可以优先从腾讯网获取)
	void GetStocksDatas(const TArray<FString>& inStocksCodes, int32 indatasource=1);
	//设置回调
	void SetCallbacks(FOnRequestSuccess successCallback, FOnRequestSuccessF10 F10successCallback, FOnRequestFailed failedCallback);
	//设置请求超时
	void SetTimeOut(float timeOutSeconds);
	//设置重试次数
	void SetRetryCount(int32 retryCount);

	//解析新浪财经返回的数据
	bool ParseSinaResponse(const FString& responseData, FQTStockRealTimeData& outRealTimeData);
	//解析腾讯财经返回的数据
	bool ParseTencentResponse(const FString& responseData, FQTStockRealTimeData& outRealTimeData);
	//同时解析多只股票数据
	bool ParseTencentResponse(const FString& responseData, TArray<FQTStockRealTimeData>& outRealTimeDatas);
	//解析东方财富返回的数据
	bool ParseEMResponse(const FString& responseData, FQTStockRealTimeData& outRealTimeData);
	//解析F10财务数据返回的数据
	bool ParseF10FinanceMainResponse(const FString& responseData, FQTFinancialF10Main& outRealTimeData);

private:
	//内部请求HTTP函数,发送股票实时数据请求
	void SendHttpRequest(const FString& url);
	//发送F10财务数据请求
	void SendHttpRequestForF10(const FString& url);
	//HTTP响应处理
	void OnResponseReceived(FHttpRequestPtr request, FHttpResponsePtr response, bool successful);
	//F10财务数据HTTP响应处理
	void OnResponseReceivedForF10(FHttpRequestPtr request, FHttpResponsePtr response, bool successful);
	//构建股票数据请求URL(指定数据源,0代表东方财富API,1代表腾讯API,2代表新浪API(新浪API暂时不可用))
	FString BuildStockDataUrl(const FString& inStockCode, int32 indatasource);
	//根据响应判断数据源
	int32 DetermineDataSource(FHttpResponsePtr response);
	// 编码转换函数
	static FString GBK2318ToUTF8(const TArray<uint8>& GBKBytes);

	TSharedPtr<IHttpRequest> httpRequest_;
	TSharedPtr<IHttpRequest> httpRequestF10_;
	FOnRequestSuccess onRequestSuccess_;
	FOnRequestSuccessF10 onRequestSuccessF10_;
	FOnRequestFailed onRequestFailed_;

	float requestTimeOut_;
	int32 maxRetryCount_;
	int32 currentRetryCount_;
	FString currentStockCode_;

	//请求头
	TMap<FString, FString> requestHeaders_;
	//用户代理列表
	TArray<FString> userAgentList_;

};