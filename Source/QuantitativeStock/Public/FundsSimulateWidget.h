
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RecommendStocksWidget.h"
#include "FundsSimulateWidget.generated.h"


UCLASS()
class QUANTITATIVESTOCK_API UFundsSimulateWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	/*
	根据当前的资金和持仓和日期,计算出当时的最优交易策略:
	首先保留至少资金总额的10%作为备用资金,剩余的资金用来交易.
	先卖出再买入.
	如果当前持仓的股票在推荐卖出的列表里,就卖出.如果当前持仓的股票不在推荐卖出的列表里,就继续持有.
	根据推荐排序交易.将推荐出手的股票排前30%的卖掉.推荐入手的股票如果超过5只,把当前资金可用余额按照0.08,0.14,0.2,0.26,0.32的比例分成5份,分别买入前5只股票,不满5只的,有几只买几只.
	*/
	UFUNCTION(BlueprintCallable, Category = "QT | FundsSimulate")
	void SimulateFunds(float inBalance, int inDate);
	//根据股票和日期返回当时的收盘价,如果没有数据了就返回-1
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "QT | Stock")
	float GetStockClosePriceByDate(FString stockCode, int inDate);

	//根据推荐卖出股票(推荐出手的股票排前30%的且权重值大于0的卖掉)
	UFUNCTION(BlueprintImplementableEvent, Category = "QT | Event")
	void SellRecommendedStocks(const TArray<FString>& stocks);
	//根据推荐入手的股票和可用资金余额买入
	UFUNCTION(BlueprintImplementableEvent, Category = "QT | Event")
	void BuyRecommendedStocks(const TArray<FString>& stocks);

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "QT | Assets")
	class UCompanyNameIndexWidget* companyNameIndexWidgetBP;

private:
	//加载本地K线可F10数据,计算技术指标,并根据技术指标和推荐算法计算出推荐买入和卖出的股票列表分别存放在RecommendBuyStocks_和RecommendSellStocks_里,键是股票代码,值是权重值(权重值越大越优先推荐买入或卖出)
	void CalculateRecommendedStocks(int inDate);
	//加载本地K线数据(根据日期加载当时有效的数据就行)
	bool LoadStockKLineData(FString stockCode, int inDate, FQTStockIndex& outKLineData);
	//加载本地F10数据(根据日期加载当时有效的数据就行)
	bool LoadStockF10Data(FString stockCode, int inDate, FQTFinancialF10Main& outF10Data);
	FString GetNameCode(FString stockCode);

	//从指定路径加载股票列表
	bool LoadListStocks(const FString& fileName,TArray<FString>& outStocks);

	//存放推荐入手股票列表的数组(每只股票对应一个权重,权重值越大越是优先推荐的股票,默认权重值是0,权重值小于0的相当于直接筛掉了)
	TMap<FString, float> RecommendBuyStocks_;
	//存放出手股票列表的数组,也就是持仓的股票(每只股票对应一个权重,权重值越大越是优先考虑出手的股票,默认权重值是0,权重值小于0的相当于直接筛掉了)
	TMap<FString, float> RecommendSellStocks_;

	float AnalyzeIndicatorsForSell(FString instock, const FQTStockIndex& indicatorDatas);
	float AnalyzeF10AndIndicatorsForBuy(FString instock, const FQTStockIndex& indicatorData,const FQTFinancialF10Main& inF10Data);

	//存放8大指标灯颜色的数组,每只股票对应一个8个灯的数组,可以用来显示每只股票的技术指标情况
	TMap<FString, TArray<EIndicatorColor>> StockIndicatorColors_;

	//读取公司简介文件,提取公司的主营业务和营业范围
	FString GetBusinessDescription(FString stockCode);
};
