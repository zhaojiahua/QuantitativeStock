#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QTCurveVectorActor.h"
#include "RecommendStocksWidget.generated.h"

UENUM(BlueprintType)
enum class EBusinessCategory : uint8
{
	Unknown          UMETA(DisplayName = "未知"),
	HighTech         UMETA(DisplayName = "高科技"),
	SalesAgency      UMETA(DisplayName = "销售代理/经销"),
	Manufacturing    UMETA(DisplayName = "传统制造"),
	Financial        UMETA(DisplayName = "金融"),
	Consumer         UMETA(DisplayName = "消费"),
	Energy           UMETA(DisplayName = "能源"),
	Other            UMETA(DisplayName = "其他")
};

UENUM(BlueprintType)
enum class EIndicatorColor : uint8
{
	None    UMETA(DisplayName = "无灯"),
	Red     UMETA(DisplayName = "红灯"),
	Green   UMETA(DisplayName = "绿灯")
};

class UStockMonitor;
UCLASS()
class QUANTITATIVESTOCK_API URecommendStocksWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	/*
	入手股票筛选步骤(由于获取数据的API限制,临时只能从最近访问列表里筛选,数目不超过100个):
	1.首先检查股票名称,凡是*ST和ST开头一律筛掉;
	2.检查公司基本面:首先检查净资产收益率ROE,ROE小于0的一律筛掉,ROE越大越优先考虑.其次再检查资产负债率,大于80%的筛掉,小于60%的优先.最后再检查现金流,小于0的筛掉,每股现金流>每股净收益的优先考虑;
	3.检查公司市值情况:市盈率PE>历史PE70%的偏高,直接筛掉,PE<历史PE30%的一般价值低估,优先考虑;
	4.宏观环境板块分析:成长繁荣期的行业板块优先考虑,比如科技板块(新能源、AI、芯片、半导体、互联网、无人机等等),
	 凡是包含设计、流片、晶圆、封装测试、EDA、IP核、制程、光刻、研发、SaaS、PaaS、云原生、中间件、算法、操作系统、机器学习、深度学习、大模型、CV、NLP、AI芯片、算法、基站、光模块、射频、天线、5G、6G、核心网、创新药、基因编辑、单抗、CAR-T、临床试验、原研、数控、机器人、自动化产线、精密制造、航天航空、碳纤维、特种合金、半导体材料、电池材料、高分子、光伏逆变器、储能BMS、固态电池、钙钛矿、氢能等这些关键词的,包含的越多越优先考虑.
	 凡是包含代理、分销、经销、进出口、供应链、系统集成、工程总包、施工、安装、加工、组装、代工、OEM、ODM、矿产、开采、冶炼、销售、门店、电商、品牌运营等这些关键词的,包含越多越次之考虑;
	5.技术指标分析:8个技术指标(包括成交量Volume在内),亮红灯的指标个数大于三个,筛掉.亮绿灯的个数大于3个可以考虑,绿灯个数越多越优先考虑;
	*/
	UFUNCTION(BlueprintCallable, Category = "QT | Stock")
	void StartFilterStocks();
	//出手股票筛选步骤:8个技术指标(包括成交量Volume在内),有两个及以上的指标亮红灯,红灯个数越多越优先考虑;
	UFUNCTION(BlueprintCallable, Category = "QT | Stock")
	void StartFilterSellStocks();
	//调用它里面的函数刷新K线数据
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "QT | Assets")
	class UCompanyNameIndexWidget* companyNameIndexWidgetBP;
	//股票推荐算法的进度,0-1,可以绑定到UI上显示进度条
	UPROPERTY(BlueprintReadWrite, Category = "QT | Params")
	float progress = 0.0f;
	//添加前10只推荐股票到UI上显示,可以绑定到UI上显示推荐股票列表
	UFUNCTION(BlueprintImplementableEvent, Category = "QT | Events")
	void DisplayRecommendedStocks(const TArray<FQTStockRealTimeData>& stockRealDatas);
	//获取推荐股票的技术指标灯颜色,可以绑定到UI上显示每只股票的技术指标情况
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "QT | Stock")
	TArray<EIndicatorColor> GetStockIndicatorColors(FString stockCode);

protected:
	virtual void NativeConstruct() override;
private:
	//从json文件中读取股票列表(由于访问限制的问题,暂时只从最近访问列表中读取股票数据),并根据传入的过滤字符过滤掉不需要的股票,过滤后的结果存储在RecommendStocks_中
	bool LoadStocksFromRecentStockListJson(const TArray<FString>& filterChars);
	//从HoldingStockList.json,OutingStockList.json,WaitingOutStockList.json这三个文件中读取股票列表,合并后输出
	bool LoadStocksForSellFromJson(FString jsonFileName);
	//存放推荐股票列表的数组(每只股票对应一个权重,权重值越大越是优先推荐的股票,默认权重值是0,权重值小于0的相当于直接筛掉了)
	TMap<FString, float> RecommendStocks_;
	//存放出手股票列表的数组(每只股票对应一个权重,权重值越大越是优先考虑出手的股票,默认权重值是0,权重值小于0的相当于直接筛掉了)
	TMap<FString, float> RecommendSellStocks_;
	//存放8大指标灯颜色的数组,每只股票对应一个8个灯的数组,可以用来显示每只股票的技术指标情况
	TMap<FString, TArray<EIndicatorColor>> StockIndicatorColors_;

	//股票监控器实例
	UPROPERTY()
	TObjectPtr<UStockMonitor> stockMonitor_;//强引用，确保监控器对象在监控期间不会被垃圾回收
	//HTTP响应处理
	void HandleStockDataResponse(const FString& ResponseData, int32 insource = 1);
	void HandleF10DataResponse(const FString& ResponseData, int32 insource = 0);
	void HandleStockDataError(int32 ErrorCode, const FString& ErrorMessage);

	bool ParseF10sFinanceMainResponse(const FString& responseData, TArray<FQTFinancialF10Main>& outF10s);
	//通过股票代码获取对应的日线数据
	void GetKLineDatasByStockCode(FString stockCode);
	bool NeedToDownLoadKLineFromInternet(FString stockCode);
	bool LoadLocalKLineData(FString stockCode, TSharedPtr<FJsonObject>& outJsonObj);
	void GetF10DatasByStockCode(FString stockCode);
	bool NeedToDownLoadF10FromInternet(FString stockCode);
	bool LoadLocalF10Data(FString stockCode, TSharedPtr<FJsonObject>& outJsonObj);
	FString GetNameCode(FString stockCode);
	//记录更新K线股票的个数=RecommendStocks_.Num()时,说明所有股票的K线数据更新完了
	int GetKLineCounts;
	//记录更新F10财务数据股票的个数=RecommendStocks_.Num()时,说明所有股票的F10数据更新完了
	int GetF10Counts;
	UFUNCTION()
	void PlusGetKLineCounts(int buyOrSell = 0);//0代表入手股票,1代表出手股票
	UFUNCTION()
	void PlusGetF10Counts();
	//更新股票K线数据和F10财务数据(0代表推荐入手的股票,1代表推荐出手的股票),更新完后调用AnalyzeStockDatas函数分析数据
	void UpdateStockKLineF10(const TArray<FString> stockCodes, int buyOrSell = 0);
	//分析出手股票只需要K线数据
	void UpdateStockKLine(const TArray<FString> stockCodes, int buyOrSell = 1);
	//分析财务数据,公司经营业务以及技术指标,判断其是否适合买入
	void AnalyzeStockDatasForBuy(const TArray<FString> stockCodes);
	//分析K线指标数据,挑出适合出手的股票,更新RecommendSellStocks_数组,并调用DisplayRecommendedStocks事件把推荐的股票列表显示在UI上
	void AnalyzeSellStockDatas(const TArray<FString> stockCodes);
	//  辅助函数：根据日期获取对应时期的EPS
	float GetEPSByDate(const TArray<TSharedPtr<FJsonValue>>* F10Datas, int32 KLineDate);

	//读取公司简介文件,提取公司的主营业务和营业范围
	FString GetBusinessDescription(FString stockCode);
public:
	/**
	 * 分析公司主营业务，返回业务类型和权重调整值
	 * @param BusinessDescription 公司主营业务描述（可以是字符串数组或单个字符串）
	 * @param OutCategory 输出业务类型
	 * @return 权重调整值（正数增加权重，负数减少权重）
	 */
	static float AnalyzeBusinessAndGetWeightAdjustment(const FString& BusinessDescription, EBusinessCategory& OutCategory);

	/*
	Volume历史百分位>0.7闪红灯,<0.3闪绿灯,否则不闪灯;
	MACD>0.2闪红灯,<-0.2闪绿灯,否则不闪灯;
	KDJ_J>90闪红灯,<10闪绿灯;
	RSI2>RSI3&&RSI2>50闪红灯,RSI2<RSI3&&RSI2<50闪绿灯,否则不闪灯;
	WR1和WR2同时大于90闪红灯,同时小于10闪绿灯,否则不闪灯;
	PDI>NDI&&ADX>20闪红灯,NDI>PDI&&ADX>20闪绿灯,否则不闪灯;
	CCI>100闪红灯,<0闪绿灯,否则不闪灯;
	BIAS0>3&&BIAS1>5&&BIAS2>10闪红灯,BIAS0<-3&&BIAS1<-5&&BIAS2<-10闪绿灯,否则不闪灯
	----------------------------------------------------------------------------------------------------------
	8个灯,红灯个数>4的直接筛掉,绿灯个数越多的,推荐权重越大
	
	分析单只股票的推荐出手*/
	float AnalyzeIndicatorsForSell(const FString& stockCode, const FQTStockIndex& Indicators);
	//分析单只股票的推荐入手(传入当天的K线数据和当天的F10财务数据)
	float AnalyzeStockForBuy(FString instock, const FQTStockIndex& indicatorData, const FQTFinancialF10Main& inF10Data);
	//辅助函数:加载最新日期KLine数据
	bool LoadLatestTechnicalIndicators(const FString& stockCode, FQTStockIndex& klineIndicators);
	//辅助函数:加载最新日期的F10财务数据
	bool LoadLatestF10Datas(const FString& stockCode, FQTFinancialF10Main& f10Datas);
	// 判断各个指标并返回灯的颜色
	static EIndicatorColor CheckVolumeIndicator(float HistoryVolumeRatio);
	static EIndicatorColor CheckMACDIndicator(float MACD);
	static EIndicatorColor CheckKDJIndicator(float KDJ_J);
	static EIndicatorColor CheckRSIIndicator(float RSI2, float RSI3);
	static EIndicatorColor CheckWRIndicator(float WR1, float WR2);
	static EIndicatorColor CheckDMIIndicator(float PDI, float NDI, float ADX);
	static EIndicatorColor CheckCCIIndicator(float CCI);
	static EIndicatorColor CheckBIASIndicator(float BIAS0, float BIAS1, float BIAS2);
	// 计算权重
	static float CalculateWeight(int32 RedLightCount, int32 GreenLightCount);
	// 计算出手权重
	static float CalculateWeightForSell(int32 RedLightCount, int32 GreenLightCount);

	// 高科技关键词库
	static const TArray<FString> HighTechKeywords;
	// 销售代理/经销关键词库
	static const TArray<FString> SalesAgencyKeywords;
	// 计算匹配分数
	static float CalculateMatchScore(const FString& Text, const TArray<FString>& Keywords);
};
