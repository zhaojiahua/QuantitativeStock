// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QTCurveVectorActor.generated.h"

class UCurveVector;
UCLASS()
class QUANTITATIVESTOCK_API AQTCurveVectorActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AQTCurveVectorActor();

	UPROPERTY(EditDefaultsOnly, Category = "QT")
	UCurveVector* vectorCrv;
	UPROPERTY(EditDefaultsOnly, Category = "QT")
	UDataTable* dataTable;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite, Category = "QT")
	int samplingCounts = 200;//采样密度默认是200个点,这个采样点个数会直接决定卷积计算的步长
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason)override;

	UFUNCTION(BlueprintCallable, Category = "QT")
	void StartConvolution();

private:
	//用于存储曲线最原始的形状,用于在程序退出的时候还原所有的曲线
	FRichCurve orgCrv0, orgCrv1, orgCrv2;
	//计算好三根曲线的定义域范围存储起来,防止在Tick函数里重复计算
	float orgTimeRange0, orgTimeRange1, orgTimeRange2;
	//沿x轴向平均采样200个点,每两个点之间的间隔
	float orgTimeStep0, orgTimeStep1, orgTimeStep2;
	//用来存储卷积结果函数的200个采样点
	TArray<FVector2f> convolutionResult;

	bool startConvolution = false;

	float timeDuration = 0.0f;

	//计算两个函数的静态卷积结果,传入两个FRichCurve曲线分别代表两个函数,经过计算后输出一个float,代表卷积的计算结果.(crv0是用来卷积的函数,crv1是被卷积的函数)
	float FixedConvolution(FRichCurve crv0, FRichCurve crv1);

	//计算并存储移动平均线到DataTable里
	void CaculateAndStoreMAIntoDataTable();
};

//股票日线数据结构体
USTRUCT(BlueprintType)
struct FQTStockIndex : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	int Date=0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString IndexCode="";
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString IndexChineseNameFull="";
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString IndexChineseName = "";
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString IndexEnglishNameFull = "";
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString IndexEnglishName = "";
	//开盘价
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float Open = 0.0f;
	//最高价
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float High = 0.0f;
	//最低价
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float Low = 0.0f;
	//收盘价
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float Close = 0.0f;
	//涨跌额
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float Change = 0.0f;
	//涨跌幅
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float ChangeRatio = 0.0f;
	//成交量
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float Volume = 0.0f;
	//成交额
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float Turnover = 0.0f;
	//振幅
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float PriceRange = 0.0f;
	//换手率
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float TurnoverRate = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	int ConsNumber = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float SMA5 = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float SMA5SUM = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float SMA10 = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float SMA10SUM = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float SMA20 = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float SMA20SUM = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float SMA60 = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float SMA60SUM = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float SMA240 = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float SMA240SUM = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float EMA5 = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float EMA10 = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float DIF1 = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float EMA20 = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float DIF2 = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float EMA60 = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float EMA240 = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float DIF = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float DEA = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float MACD = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float BollUpper = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float BollLower = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float KDJ_RSV = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float KDJ_K= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float KDJ_D= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float KDJ_J= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float RSI0_AVGUp= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float RSI0_AVGDown= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float RSI0= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float RSI1_AVGUp= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float RSI1_AVGDown= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float RSI1= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float RSI2_AVGUp= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float RSI2_AVGDown= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float RSI2= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float WR1= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float WR2= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float TR_Average= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float PDI_Average= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float NDI_Average= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float PDI= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float NDI= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float DX= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float ADX= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float ADXR= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float CCI_TP= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float CCI_TPSUM= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float CCI_SMA= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float CCI_MAD= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float CCI= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float BIAS0_SMASUM= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float BIAS1_SMASUM= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float BIAS2_SMASUM= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float BIAS0= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float BIAS1= 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float BIAS2= 0.0f;
};

//公司的概况信息数据结构体
USTRUCT(BlueprintType)
struct FQTCompanyAbstractRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString CompanyName = "";
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString EnglishName = "";
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString OldSimplifyName = "";
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString IndexCodeA = "";
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString SimplifyNameA = "";
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString IndexCodeB = "";
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString SimplifyNameB = "";
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString IndexCodeH = "";
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString SimplifyNameH = "";
	//所属指数
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString InclusionIndex = "";
	//所属市场板块
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString MarketSegment = "";
	//所属行业
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString Industry = "";
	//法定代表人
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString LegalRepresentative = "";
	//注册资金
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString RegisteredCapital = "";
	//成立日期
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString EstablishmentDate = "";
	//上市日期
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString ListingDate = "";
	//公司官网
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString OfficialWebsite = "";
	//电子邮箱
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString Email = "";
	//公司电话
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString Telephone = "";
	//公司传真
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString Fax = "";
	//注册地址
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString RegisteredAddress = "";
	//办公地址
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString OfficeAddress = "";
	//邮政编码
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString PostalCode = "";
	//主营业务
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString MainBusiness = "";
	//经营范围
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString ScopeOfBusiness = "";
	//机构简介
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString OrganizationIntroduction = "";

};

//股票列表数据结构体(用于检索信息,以便后续使用)
USTRUCT(BlueprintType)
struct FQTStockListRow : public FTableRowBase
{
	GENERATED_BODY()
	//股票代码
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString CODE = "";
	//股票简称
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString NAME = "";
	//股票代码+市场后缀(用于搜索公司概况的API使用)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString CODEMARK = "";
	//股票简称+股票代码(用于生成文件名使用)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString  NAMECODE = "";
	//基金类型
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString  FUNDTYPE = "";
	//所属市场
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString  MARK = "";
};

//财务指标数据结构体
USTRUCT(BlueprintType)
struct FQTFinancialF10Main : public FTableRowBase
{
	GENERATED_BODY()
	//证券代码
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString SECURITY_CODE = "";
	//证券简称
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString SECURITY_NAME_ABBR = "";
	//报告期
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString REPORT_DATE = "";
	//报告类型
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString REPORT_TYPE = "";
	//报告期名称
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString REPORT_DATE_NAME = "";
	//公告日期
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString NOTICE_DATE = "";
	//更新日期
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString UPDATE_DATE = "";
	//货币
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString CURRENCY = "";
	//基本每股收益(元)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float EPSJB = 0.0f;
	//扣非每股收益(元)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float EPSKCJB = 0.0f;
	//稀释每股收益(元)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float EPSXS = 0.0f;
	//每股净资产(元)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float BPS = 0.0f;
	//每股公积金(元)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float MGZBGJ = 0.0f;
	//每股未分配利润(元)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float MGWFPLR = 0.0f;
	//每股经营现金流(元)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float MGJYXJJE = 0.0f;
	//毛利润(元)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float MLR = 0.0f;
	//归属母公司股东的净利润(元)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float PARENTNETPROFIT = 0.0f;
	//扣除非经常性损益后的净利润(元)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float KCFJCXSYJLR = 0.0f;
	//营业总收入同比增长率(%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float TOTALOPERATEREVETZ = 0.0f;
	//归属母公司股东的净利润同比增长率(%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float PARENTNETPROFITTZ = 0.0f;
	//扣除非经常性损益后的净利润同比增长率(%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float KCFJCXSYJLRTZ = 0.0f;
	//营业总收入滚动环比增长(%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float YYZSRGDHBZC = 0.0f;
	//归属母公司股东的净利润滚动环比增长(%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float NETPROFITRPHBZC = 0.0f;
	//扣非净利润滚动环比增长(%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float KFJLRGDHBZC = 0.0f;
	//净资产收益率ROE(加权)(%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float ROEJQ = 0.0f;
	//净资产收益率ROE(扣非/加权)(%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float ROEKCJQ = 0.0f;
	//总资产报酬率ROA(加权)(%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float ZZCJLL = 0.0f;
	//净利率(%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float XSJLL = 0.0f;
	//毛利率(%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float XSMLL = 0.0f;
	//预收账款/营业收入(%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float YSZKYYSR = 0.0f;
	//销售净现金流/营业收入(%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float XSJXLYYSR = 0.0f;
	//经营净现金流/营业收入(%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float JYXJLYYSR = 0.0f;
	//实际税率(%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float TAXRATE = 0.0f;
	//流动比率(%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float LD = 0.0f;
	//速动比率(%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float SD = 0.0f;
	//现金流量比率(%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float XJLLB = 0.0f;
	//资产负债率(%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float ZCFZL = 0.0f;
	//权益系数(%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float QYCS = 0.0f;
	//产权比率(%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float CQBL = 0.0f;
	//总资产周转天数(天)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float ZZCZZTS = 0.0f;
	//存货周转天数(天)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float CHZZTS = 0.0f;
	//应收账款周转天数(天)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float YSZKZZTS = 0.0f;
	//总资产周转率(次)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float TOAZZL = 0.0f;
	//存货周转率(次)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float CHZZL = 0.0f;
	//应收账款周转率(次)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float YSZKZZL = 0.0f;
	//资产总计
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float TOTAL_ASSETS = 0.0f;
	//流动资产合计
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float TOTAL_CURRENT_ASSETS = 0.0f;
	//非流动资产合计
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float TOTAL_NONCURRENT_ASSETS = 0.0f;
	//负债合计
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float TOTAL_LIABILITIES = 0.0f;
	//归属于母公司股东权益总计
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float TOTAL_PARENT_EQUITY = 0.0f;
	//营业总成本
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float TOTAL_OPERATE_COST = 0.0f;
	//营业总收入
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float TOTAL_OPERATE_INCOME = 0.0f;
};

//股票实时数据结构体
USTRUCT(BlueprintType)
struct FQTStockRealTimeData : public FTableRowBase
{
	GENERATED_BODY()
	//股票代码
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString StockCode = "";
	//股票名称
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString StockName = "";
	//最新价
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float LatestPrice = 0.0f;
	//涨跌额
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float ChangeAmount = 0.0f;
	//涨跌幅(%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float ChangeRatio = 0.0f;
	//成交量(手)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float Volume = 0.0f;
	//成交额(万元)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float Turnover = 0.0f;
	//换手率(%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float TurnoverRate = 0.0f;
	//量比(%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float VolumeRatio = 0.0f;
	//振幅(%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float PriceRange = 0.0f;
	//今开
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float OpenPrice = 0.0f;
	//昨收
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float PreviousClosePrice = 0.0f;
	//最高
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float HighestPrice = 0.0f;
	//最低
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float LowestPrice = 0.0f;
	//买一价
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float BuyOnePrice = 0.0f;
	//买二价
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float BuyTwoPrice = 0.0f;
	//买三价
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float BuyThreePrice = 0.0f;
	//卖一价
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float SellOnePrice = 0.0f;
	//卖二价
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float SellTwoPrice = 0.0f;
	//卖三价
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float SellThreePrice = 0.0f;
	//市盈率
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float PE_Ratio = 0.0f;
	//市净率
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float PB_Ratio = 0.0f;
	//总市值(亿元)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float TotalMarketValue = 0.0f;
	//流通市值(亿元)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float CirculatingMarketValue = 0.0f;
	//涨停价
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float UpperLimitPrice = 0.0f;
	//跌停价
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float LowerLimitPrice = 0.0f;

	//更新时间
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString UpdateTime = "";
};