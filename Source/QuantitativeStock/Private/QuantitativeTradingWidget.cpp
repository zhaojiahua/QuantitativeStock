
#include "QuantitativeTradingWidget.h"
#include "StockMonitor.h"
#include "QTCurveVectorActor.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Misc/DefaultValueHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void UQTTreeViewItemObj::AddSubItem(UQTTreeViewItemObj* subItem)
{
	if (subItem)subItems.Add(subItem);
}

UQTTreeViewItemObj* UQuantitativeTradingWidget::BuildUQTTreeViewItemObj(FString inname, int indepth)
{
	UQTTreeViewItemObj* tempObj = NewObject<UQTTreeViewItemObj>();
	tempObj->itemName = inname;
	tempObj->depth = indepth;
	return tempObj;
}

int UQuantitativeTradingWidget::GetIndexFromItemString(FString inString){
	if (inString.Equals("Nearest1Mouths")) return 1;
	if (inString.Equals("Nearest2Mouths")) return 2;
	if (inString.Equals("Nearest3Mouths")) return 3;
	if (inString.Equals("Nearest4Mouths")) return 4;
	if (inString.Equals("Nearest5Mouths")) return 5;
	if (inString.Equals("Nearest6Mouths")) return 6;
	if (inString.Equals("Nearest1Years")) return 10000;
	if (inString.Equals("Nearest2Years")) return 20000;
	if (inString.Equals("Nearest3Years")) return 30000;
	if (inString.Equals("Nearest4Years")) return 40000;
	if (inString.Equals("Nearest5Years")) return 50000;
	return 10000;
}

void UQuantitativeTradingWidget::NativePreConstruct(){
	Super::NativePreConstruct();
	monitorInterval_ = 5.0f;
	stockMonitor_ = nullptr;
}

void UQuantitativeTradingWidget::NativeConstruct(){
	Super::NativeConstruct();
	stockMonitor_ = NewObject<UStockMonitor>(this);
	if(stockMonitor_){
		stockMonitor_->SetTimeOut(10.0f);
		stockMonitor_->SetRetryCount(3);
		stockMonitor_->SetCallbacks(
			FOnRequestSuccess::CreateUObject(this, &UQuantitativeTradingWidget::HandleStockDataResponse),
			FOnRequestSuccessF10::CreateUObject(this, &UQuantitativeTradingWidget::HandleF10DataResponse),
			FOnRequestFailed::CreateUObject(this, &UQuantitativeTradingWidget::HandleStockDataError)
		);
	}
}

void UQuantitativeTradingWidget::NativeDestruct(){
	StopMonitoring();
	Super::NativeDestruct();
}

void UQuantitativeTradingWidget::GetLatestF10FinanceData(const FString& StockCode, int32 insource){
	if (!stockMonitor_) stockMonitor_ = NewObject<UStockMonitor>();
	stockMonitor_->GetStockF10FianceMainData(StockCode, insource);
}

void UQuantitativeTradingWidget::StartMonitoring(const FString& inStockCode, float inIntervalSeconds, int32 insource){
	if (isMonitoring_) return;
	if (inStockCode.IsEmpty()) return;
	if (!stockMonitor_) {
		stockMonitor_ = NewObject<UStockMonitor>();
	}
	currentStockCode_ = inStockCode;
	monitorInterval_ = FMath::Max(1.0f, inIntervalSeconds);
	latestStockDataMap_.Empty();
	isMonitoring_ = true;
	insource_ = insource;
	//开始首次更新
	UpdateStockData();
	//启动定时器
	GetWorld()->GetTimerManager().SetTimer(monitorTimerHandle_, this, &UQuantitativeTradingWidget::UpdateStockData, monitorInterval_, true, 0.0f);
	UE_LOG(LogTemp, Log, TEXT("---------------------------------------------------->> 开始监控股票: %s, 间隔: %.1f秒"), *currentStockCode_, monitorInterval_);
}
void UQuantitativeTradingWidget::StopMonitoring() {
	if (!isMonitoring_) return;
	isMonitoring_ = false;
	currentStockCode_.Empty();
	latestStockDataMap_.Empty();
	//清除定时器
	GetWorld()->GetTimerManager().ClearTimer(monitorTimerHandle_);
	UE_LOG(LogTemp, Log, TEXT("---------------------------------------------------->> 停止监控股票"));
}

void UQuantitativeTradingWidget::StartMonitoringMultiple(const TArray<FString>& StockCodes, float IntervalSeconds){
	if (isMonitoring_) StopMonitoring();
	if (StockCodes.Num() == 0) return;
	if (!stockMonitor_) {
		stockMonitor_ = NewObject<UStockMonitor>();
	}
	allStockCodes_ = StockCodes;
	monitorInterval_ = FMath::Max(1.0f, IntervalSeconds);
	isMonitoring_ = true;
	//开始首次更新
	UpdateStockData();
	//启动定时器
	GetWorld()->GetTimerManager().SetTimer(monitorTimerHandle_, this, &UQuantitativeTradingWidget::UpdateStockData, monitorInterval_, true, 0.0f);
	UE_LOG(LogTemp, Log, TEXT("---------------------------------------------------->> 开始监控多只股票, 数量: %d, 间隔: %.1f秒"), allStockCodes_.Num(), monitorInterval_);
}

FQTStockRealTimeData UQuantitativeTradingWidget::GetLatestStockData() const{
	if (latestStockDataMap_.Contains(currentStockCode_)) {
		return latestStockDataMap_[currentStockCode_];
	}
	return FQTStockRealTimeData();
}

TArray<FQTStockRealTimeData> UQuantitativeTradingWidget::GetAllLatestStockData() const{
	TArray<FQTStockRealTimeData> resultArray;
	latestStockDataMap_.GenerateValueArray(resultArray);
	return resultArray;
}

void UQuantitativeTradingWidget::UpdateStockData(){
	if (!isMonitoring_) return;
	if (!stockMonitor_) return;
	if (allStockCodes_.Num() > 0) {
		stockMonitor_->GetStocksDatas(allStockCodes_, insource_);
	}
	else if (!currentStockCode_.IsEmpty()) {
		stockMonitor_->GetStockData(currentStockCode_, insource_);
	}
}

void UQuantitativeTradingWidget::HandleStockDataResponse(const FString& ResponseData, int32 insource) {
	if (!isMonitoring_) return;
	UE_LOG(LogTemp, Warning, TEXT("---------------------------->> 接收到数据开始解析"));
	FQTStockRealTimeData stockData;
	bool bParseSuccess = false;
	if (insource == 0) bParseSuccess = stockMonitor_->ParseEMResponse(ResponseData, stockData);
	else if (insource == 1) bParseSuccess = stockMonitor_->ParseTencentResponse(ResponseData, stockData);
	else if (insource == 2) bParseSuccess = stockMonitor_->ParseSinaResponse(ResponseData, stockData);
	if (bParseSuccess) {
		if (!historicalStockDataMap_.Contains(stockData.StockCode)) {
			historicalStockDataMap_.Add(stockData.StockCode, TArray<FQTStockRealTimeData>());
		}
		TArray<FQTStockRealTimeData>& historyArray = historicalStockDataMap_[stockData.StockCode];
		historyArray.Add(stockData);
		//只保留最近100条历史数据
		if (historyArray.Num() > 100) {
			historyArray.RemoveAt(0, historyArray.Num() - 100);
		}
		//更新最新数据
		latestStockDataMap_.Add(stockData.StockCode, stockData);
		//触发数据更新事件
		onStockRealTimeDataUpdated.Broadcast(stockData);
		// 打印日志
		UE_LOG(LogTemp, Log, TEXT("---------------------------->> 股票数据更新: %s %s - 价格: %.2f (%.2f%%)"), *stockData.StockCode, *stockData.StockName, stockData.LatestPrice, stockData.ChangeRatio);
	}
	else 	UE_LOG(LogTemp, Warning, TEXT("---------------------------->> 解析股票数据失败: %s"), *ResponseData);
}

void UQuantitativeTradingWidget::HandleF10DataResponse(const FString& responseData, int32 insource){
	UE_LOG(LogTemp, Warning, TEXT("---------------------------->> 接收到F10数据开始解析"));
	// 直接触发F10数据更新事件
	bool bParseSuccess = stockMonitor_->ParseF10FinanceMainResponse(responseData, f10Data);
	onF10Updated.Broadcast(f10Data);
	if (bParseSuccess) {
		UE_LOG(LogTemp, Log, TEXT("---------------------------->> F10财务数据更新: %s %s - 报告期: %s"), *f10Data.SECURITY_CODE, *f10Data.SECURITY_NAME_ABBR, *f10Data.REPORT_DATE);
	}
	else 	UE_LOG(LogTemp, Warning, TEXT("---------------------------->> 解析F10财务数据失败: %s"), *responseData);
}

void UQuantitativeTradingWidget::HandleStockDataError(int32 ErrorCode, const FString& ErrorMessage){
	if (!isMonitoring_) return;
	UE_LOG(LogTemp, Error, TEXT("股票数据请求错误: %d - %s"), ErrorCode, *ErrorMessage);
	onStockMonitorError.Broadcast(currentStockCode_, ErrorMessage);
}

