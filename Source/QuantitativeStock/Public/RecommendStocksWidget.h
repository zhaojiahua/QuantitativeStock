#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RecommendStocksWidget.generated.h"

struct FQTFinancialF10Main;

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
	出手股票筛选步骤:8个技术指标(包括成交量Volume在内),有两个及以上的指标亮红灯,红灯个数越多越优先考虑;
	*/
	UFUNCTION(BlueprintCallable, Category = "Stock")
	void StartFilterStocks();
	//调用它里面的函数刷新K线数据
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "QT | Assets")
	class UCompanyNameIndexWidget* companyNameIndexWidgetBP;

protected:
	virtual void NativeConstruct() override;
private:
	//从json文件中读取股票列表(由于访问限制的问题,暂时只从最近访问列表中读取股票数据),并根据传入的过滤字符过滤掉不需要的股票,过滤后的结果存储在RecommendStocks_中
	bool LoadStocksFromRecentStockListJson(const TArray<FString>& filterChars);
	//存放推荐股票列表的数组(每只股票对应一个权重,权重值越大越是优先推荐的股票,默认权重值是0,权重值小于0的相当于直接筛掉了)
	TMap<FString, float> RecommendStocks_;

	//股票监控器实例
	class UStockMonitor* stockMonitor_;
	//HTTP响应处理
	void HandleStockDataResponse(const FString& ResponseData, int32 insource = 0);
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
	void PlusGetKLineCounts();
	UFUNCTION()
	void PlusGetF10Counts();
	//更新股票K线数据和F10财务数据
	void UpdateStockKLineF10(const TArray<FString> stockCodes);
	//分析财务数据
	void AnalyzeF10Datas(const TArray<FString> stockCodes);
	//  辅助函数：根据日期获取对应时期的EPS
	float GetEPSByDate(const TArray<TSharedPtr<FJsonValue>>* F10Datas, int32 KLineDate);

	//读取公司简介文件,提取公司的主营业务和营业范围
	FString GetBusinessDescription(FString stockCode);

	/**
	 * 分析公司主营业务，返回业务类型和权重调整值
	 * @param BusinessDescription 公司主营业务描述（可以是字符串数组或单个字符串）
	 * @param OutCategory 输出业务类型
	 * @return 权重调整值（正数增加权重，负数减少权重）
	 */
	static float AnalyzeBusinessAndGetWeightAdjustment(const FString& BusinessDescription, EBusinessCategory& OutCategory);

	// 高科技关键词库
	static const TArray<FString> HighTechKeywords;
	// 销售代理/经销关键词库
	static const TArray<FString> SalesAgencyKeywords;
	// 计算匹配分数
	static float CalculateMatchScore(const FString& Text, const TArray<FString>& Keywords);
};
