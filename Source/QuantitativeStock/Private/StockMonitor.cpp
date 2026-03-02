#include "StockMonitor.h"
#include "QTCurveVectorActor.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Base64.h"
#include "Misc/DefaultValueHelper.h"
#include "TimerManager.h"
#include "Engine/World.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include <stringapiset.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

UStockMonitor::UStockMonitor(){
	requestTimeOut_ = 10.0f;
	maxRetryCount_ = 3;
	currentRetryCount_ = 0;
	//初始化用户代理列表
	userAgentList_ = {
		TEXT("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.3"),
		TEXT("Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/56.0.2924.87 Safari/537.36"),
		TEXT("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_12_6) AppleWebKit/602.3.12 (KHTML, like Gecko) Version/10.0.3 Safari/602.4.8"),
		TEXT("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/44.0.2403.157 Safari/537.36"),
		TEXT("Mozilla/5.0 (iPhone; CPU iPhone OS 10_3_1 like Mac OS X) AppleWebKit/603.1.30 (KHTML, like Gecko) Version/10.0 Mobile/14E304 Safari/602.1"),
		TEXT("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"),
		TEXT("Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:121.0) Gecko/20100101 Firefox/121.0"),
		TEXT("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"),
		TEXT("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36")
	};
	// 基础头
	requestHeaders_.Add(TEXT("Accept"), TEXT("text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8"));
	requestHeaders_.Add(TEXT("Accept-Language"), TEXT("zh-CN,zh;q=0.9,en;q=0.8"));
	requestHeaders_.Add(TEXT("Accept-Encoding"), TEXT("gzip, deflate"));
	requestHeaders_.Add(TEXT("Connection"), TEXT("keep-alive"));
	requestHeaders_.Add(TEXT("Cache-Control"), TEXT("no-cache"));
	requestHeaders_.Add(TEXT("Pragma"), TEXT("no-cache"));
	requestHeaders_.Add(TEXT("Upgrade-Insecure-Requests"), TEXT("1"));
	// 模拟浏览器头
	requestHeaders_.Add(TEXT("Sec-Fetch-Dest"), TEXT("document"));
	requestHeaders_.Add(TEXT("Sec-Fetch-Mode"), TEXT("navigate"));
	requestHeaders_.Add(TEXT("Sec-Fetch-Site"), TEXT("same-origin"));
	requestHeaders_.Add(TEXT("Sec-Fetch-User"), TEXT("?1"));
}

void UStockMonitor::GetStockF10FianceMainData(const FString& inStockCode, int32 indatasource){
	//最新一期资产负债表和利润表摘要的指定字段数据
	//获取资产负债表利润表摘要数据的URL-东方财富网站API
	FString urlGRATIO = FString::Printf(TEXT("https://datacenter.eastmoney.com/securities/api/data/v1/get?reportName=RPT_F10_FINANCE_GRATIO&columns=TOTAL_ASSETS,TOTAL_CURRENT_ASSETS,TOTAL_NONCURRENT_ASSETS,TOTAL_LIABILITIES,TOTAL_OPERATE_COST,TOTAL_OPERATE_INCOME&filter=(SECUCODE%%3D%%22%s.%s%%22)&sortTypes=-1%%2C1&sortColumns=REPORT_DATE%%2CINTERFACE_TYPE&pageNumber=1&pageSize=1&source=HSF10"), *inStockCode, inStockCode.StartsWith(TEXT("6")) ? TEXT("SH") : TEXT("SZ"));
	//获取主要财务指标数据的URL-东方财富网站API
	FString urlMainIndicator = FString::Printf(TEXT("https://datacenter.eastmoney.com/securities/api/data/get?type=RPT_F10_FINANCE_MAINFINADATA&sty=APP_F10_MAINFINADATA&filter=(SECUCODE%%3D%%22%s.%s%%22)&p=1&ps=1&sr=-1&st=REPORT_DATE&source=HSF10"), *inStockCode, inStockCode.StartsWith(TEXT("6")) ? TEXT("SH") : TEXT("SZ"));
	if (indatasource == 1) {
		// 腾讯API暂不支持获取F10财务数据
		UE_LOG(LogTemp, Error, TEXT("--------------------->> 腾讯API暂不支持获取F10财务数据"));
	}
	else if (indatasource == 2) {
		// 新浪API暂不支持获取F10财务数据
		UE_LOG(LogTemp, Error, TEXT("--------------------->> 新浪API暂不支持获取F10财务数据"));
	}
	SendHttpRequestForF10(urlGRATIO);
	SendHttpRequestForF10(urlMainIndicator);
	//等待响应完成(简化处理，实际应用中应使用异步回调)
}

void UStockMonitor::GetStockData(const FString& inStockCode, int32 indatasource){
	currentStockCode_ = inStockCode;
	currentRetryCount_ = 0;
	FString url = BuildStockDataUrl(inStockCode, indatasource);
	SendHttpRequest(url);
}

void UStockMonitor::GetStocksDatas(const TArray<FString>& inStocksCodes, int32 indatasource){
	if (inStocksCodes.Num() == 0)  return;
	if(indatasource == 0){
		// 东方财富API只能单只获取，多只只能通过循环或并发
		UE_LOG(LogTemp, Warning, TEXT("腾讯API只能单只获取，多只只能通过循环或并发"));
		return;
	}
	else if (indatasource == 1) {//从腾讯网下载数据
		FString codeList;
		for (const FString& stockCode : inStocksCodes) {
			FString prefix = stockCode.StartsWith(TEXT("6")) ? TEXT("sh") : TEXT("sz");
			codeList += prefix + stockCode + TEXT(",");
		}
		codeList.RemoveFromEnd(TEXT(","));
		FString url = FString::Printf(TEXT("http://qt.gtimg.cn/q=%s"), *codeList);
		SendHttpRequest(url);
	}
}

void UStockMonitor::SetCallbacks(FOnRequestSuccess successCallback, FOnRequestSuccessF10 F10successCallback, FOnRequestFailed failedCallback){
	onRequestSuccess_ = successCallback;
	onRequestSuccessF10_ = F10successCallback;
	onRequestFailed_ = failedCallback;
}

void UStockMonitor::SetTimeOut(float timeOutSeconds){
	requestTimeOut_ = FMath::Max(1.0f, timeOutSeconds);
}

void UStockMonitor::SetRetryCount(int32 retryCount){
	maxRetryCount_ = FMath::Max(0, retryCount);
}

void UStockMonitor::SendHttpRequest(const FString& url){
	//创建HTTP请求
	httpRequest_ = FHttpModule::Get().CreateRequest();
	if(!httpRequest_.IsValid()){
		if (onRequestFailed_.IsBound()) onRequestFailed_.Execute(500, TEXT("创建HTTP请求失败!"));
		return;
	}
	//设置请求方法和URL
	httpRequest_->SetURL(url);
	httpRequest_->SetVerb(TEXT("GET"));
	httpRequest_->SetTimeout(requestTimeOut_);
	//设置请求头
	for (const TPair<FString, FString>& header : requestHeaders_) {
		httpRequest_->SetHeader(header.Key, header.Value);
	}
	//随机设置用户代理
	int32 randomIndex = FMath::RandRange(0, userAgentList_.Num() - 1);
	httpRequest_->SetHeader(TEXT("User-Agent"), userAgentList_[randomIndex]);
	//绑定响应处理函数
	httpRequest_->OnProcessRequestComplete().BindUObject(this, &UStockMonitor::OnResponseReceived);
	//发送请求
	if(!httpRequest_->ProcessRequest()){
		if (onRequestFailed_.IsBound()) onRequestFailed_.Execute(500, TEXT("发送HTTP请求失败!"));
		return;
	}
}

void UStockMonitor::SendHttpRequestForF10(const FString& inurl){
	//创建HTTP请求
	httpRequestF10_ = FHttpModule::Get().CreateRequest();
	if (!httpRequestF10_.IsValid()) {
		if (onRequestFailed_.IsBound()) onRequestFailed_.Execute(500, TEXT("创建HTTPForF10请求失败!"));
		return;
	}
	//设置请求方法和URL
	httpRequestF10_->SetURL(inurl);
	httpRequestF10_->SetVerb(TEXT("GET"));
	httpRequestF10_->SetTimeout(requestTimeOut_);
	//设置请求头
	for (const TPair<FString, FString>& header : requestHeaders_) {
		httpRequestF10_->SetHeader(header.Key, header.Value);
	}
	//随机设置用户代理
	int32 randomIndex = FMath::RandRange(0, userAgentList_.Num() - 1);
	httpRequestF10_->SetHeader(TEXT("User-Agent"), userAgentList_[randomIndex]);
	//绑定响应处理函数
	httpRequestF10_->OnProcessRequestComplete().BindUObject(this, &UStockMonitor::OnResponseReceivedForF10);
	//发送请求
	if (!httpRequestF10_->ProcessRequest()) {
		if (onRequestFailed_.IsBound()) onRequestFailed_.Execute(500, TEXT("发送HTTP请求失败!"));
		return;
	}
}

void UStockMonitor::OnResponseReceived(FHttpRequestPtr request, FHttpResponsePtr response, bool successful){
	if (!successful || !response.IsValid()) { UE_LOG(LogTemp, Error, TEXT("--------------------------->> HTTP请求失败")); return; }
	int32 responseCode = response->GetResponseCode();
	FString responseContent = response->GetContentAsString();
	if (responseCode == 403) { 
		UE_LOG(LogTemp, Warning, TEXT("--------------------------->> HTTP请求被拒绝 403"));
		//查找具体原因
		FString serverHeader = response->GetHeader(TEXT("Server"));
		if (serverHeader.Contains(TEXT("cloudflare")))UE_LOG(LogTemp, Warning, TEXT("--------------------------->> CloudFlare防护触发"));
		return;
	}
	if (responseCode == 404) { UE_LOG(LogTemp, Warning, TEXT("--------------------------->> HTTP请求未找到 404")); return; }
	if (responseCode == 200) {
		UE_LOG(LogTemp, Warning, TEXT("--------------------------->> HTTP请求成功 %d"), responseCode);
		currentRetryCount_ = 0;
		int32 dataSource = DetermineDataSource(response);
		if (dataSource >= 0 && onRequestSuccess_.IsBound()) onRequestSuccess_.Execute(GBK2318ToUTF8(response->GetContent()), dataSource);//触发成功回调
		return;
	}
	//请求失败，检查是否需要重试
	if (currentRetryCount_ < maxRetryCount_) {
		currentRetryCount_++;
		//重新发送请求
		SendHttpRequest(request->GetURL());
	}
	else {
		//达到最大重试次数，触发失败回调
		if (onRequestFailed_.IsBound()) {
			int32 errorCode = response.IsValid() ? response->GetResponseCode() : -1;
			FString errorMessage = response.IsValid() ? response->GetContentAsString() : TEXT("请求失败且无响应!");
			onRequestFailed_.Execute(errorCode, errorMessage);
		}
	}
}

void UStockMonitor::OnResponseReceivedForF10(FHttpRequestPtr request, FHttpResponsePtr response, bool successful){
	if (!successful || !response.IsValid()) { UE_LOG(LogTemp, Error, TEXT("--------------------------->> HTTPForF10请求失败")); return; }
	int32 responseCode = response->GetResponseCode();
	FString responseContent = response->GetContentAsString();
	if (responseCode == 403) {
		UE_LOG(LogTemp, Warning, TEXT("--------------------------->> HTTPForF10请求被拒绝 403"));
		//查找具体原因
		FString serverHeader = response->GetHeader(TEXT("Server"));
		if (serverHeader.Contains(TEXT("cloudflare")))UE_LOG(LogTemp, Warning, TEXT("--------------------------->> CloudFlare防护触发"));
		return;
	}
	if (responseCode == 404) { UE_LOG(LogTemp, Warning, TEXT("--------------------------->> HTTPForF10请求未找到 404")); return; }
	if (responseCode == 200) {
		UE_LOG(LogTemp, Warning, TEXT("--------------------------->> HTTPForF10请求成功 %d"), responseCode);
		int32 dataSource = DetermineDataSource(response);
		if (dataSource >= 0 && onRequestSuccessF10_.IsBound()) onRequestSuccessF10_.Execute(response->GetContentAsString(), dataSource);//触发成功回调
		return;
	}
	//触发失败回调
	if (onRequestFailed_.IsBound()) {
		int32 errorCode = response.IsValid() ? response->GetResponseCode() : -1;
		FString errorMessage = response.IsValid() ? response->GetContentAsString() : TEXT("请求失败且无响应!");
		onRequestFailed_.Execute(errorCode, errorMessage);
	}
}

FString UStockMonitor::BuildStockDataUrl(const FString& inStockCode, int32 indatasource){
	TArray<FString> datasources = {
		// 东方财富API（只能获取单只股票数据,获取多只的话只能通过循环或并发）
		FString::Printf(TEXT("https://push2.eastmoney.com/api/qt/stock/get?secid=%s.%s"),currentStockCode_.StartsWith(TEXT("6")) ? TEXT("1") : TEXT("0"),*currentStockCode_),
		// 腾讯备用API
		FString::Printf(TEXT("http://qt.gtimg.cn/q=%s%s"),currentStockCode_.StartsWith(TEXT("6")) ? TEXT("sh") : TEXT("sz"),*currentStockCode_),
		// 新浪备用API
		FString::Printf(TEXT("https://hq.sinajs.cn/list=%s%s"),currentStockCode_.StartsWith(TEXT("6")) ? TEXT("sh") : TEXT("sz"),*currentStockCode_)
	};
	if(indatasource ==0)return datasources[0];
	else if(indatasource ==1) return datasources[1];
	else return datasources[2];

}

int32 UStockMonitor::DetermineDataSource(FHttpResponsePtr response){
	//根据响应头或内容判断数据源
	if (!response.IsValid())return -1;
	const TArray<FString>& Headers = response->GetAllHeaders();
	FString ServerHeader;
	// 查找Server、Via、X-Powered-By等字段
	for (const FString& Header : Headers){
		if (Header.StartsWith(TEXT("Server:"))){
			ServerHeader = Header;
			break;
		}
	}
	if (ServerHeader.Contains(TEXT("eastmoney")) || ServerHeader.Contains(TEXT("EM-Router")) || ServerHeader.Contains(TEXT("emstat")) || response->GetURL().Contains(TEXT("eastmoney.com")))return 0; // 东方财富API
	else if (ServerHeader.Contains(TEXT("Tengine")) || ServerHeader.Contains(TEXT("Tencent")) || response->GetURL().Contains(TEXT("qt.gtimg.cn")) || response->GetURL().Contains(TEXT("finance.qq.com")))return 1; // 腾讯API
	else if (ServerHeader.Contains(TEXT("sina"))) return 2;// 新浪API
	return -1;
}

FString UStockMonitor::GBK2318ToUTF8(const TArray<uint8>& GBKData){
#if PLATFORM_WINDOWS
	if (GBKData.Num() == 0)
		return TEXT("");

	// 腾讯API使用GBK编码，代码页936
	int32 WideCharCount = MultiByteToWideChar(
		936,                    // ✅ 腾讯API使用GBK编码
		MB_ERR_INVALID_CHARS,
		(LPCCH)GBKData.GetData(),
		GBKData.Num(),
		NULL,
		0
	);

	if (WideCharCount == 0)
	{
		// 尝试其他方式解码
		UE_LOG(LogTemp, Warning, TEXT("GBK解码失败，尝试系统默认编码"));
		return FString(ANSI_TO_TCHAR((const char*)GBKData.GetData()));
	}

	TArray<WCHAR> WideChars;
	WideChars.SetNum(WideCharCount + 1);

	WideCharCount = MultiByteToWideChar(
		936,
		MB_ERR_INVALID_CHARS,
		(LPCCH)GBKData.GetData(),
		GBKData.Num(),
		WideChars.GetData(),
		WideCharCount
	);

	if (WideCharCount == 0)
	{
		return TEXT("");
	}

	WideChars[WideCharCount] = L'\0';
	return FString(WideChars.GetData());

#else
	// 非Windows平台
	// 尝试直接转换，可能会乱码
	UE_LOG(LogTemp, Warning, TEXT("非Windows平台，腾讯API中文可能显示乱码"));
	return FString(UTF8_TO_TCHAR((const char*)GBKData.GetData()));
#endif
}

bool UStockMonitor::ParseSinaResponse(const FString& responseData, FQTStockRealTimeData& outRealTimeData){
	// 解析新浪财经格式: var hq_str_sh600519="茅台,1920.01,1900.00,1915.50,...";
	int32 startIndex = responseData.Find(TEXT("\"")) + 1;
	int32 endIndex = responseData.Find(TEXT("\";"), ESearchCase::IgnoreCase);
	if (startIndex <= 0 || endIndex <= startIndex) return false;
	FString dataContent = responseData.Mid(startIndex, endIndex - startIndex);
	TArray<FString> dataParts;
	dataContent.ParseIntoArray(dataParts, TEXT(","), true);
	if (dataParts.Num() >= 32) {
		outRealTimeData.StockName = dataParts[0];
		float CurrentPrice, YesterdayClose;
		if (FDefaultValueHelper::ParseFloat(dataParts[3], CurrentPrice) &&	FDefaultValueHelper::ParseFloat(dataParts[2], YesterdayClose)){
			outRealTimeData.LatestPrice = CurrentPrice;
			outRealTimeData.ChangeAmount = CurrentPrice - YesterdayClose;
			return true;}
	}
	return false;
}

bool UStockMonitor::ParseTencentResponse(const FString& responseData, FQTStockRealTimeData& outRealTimeData){
	/*
	v_sh600519="1~贵州茅台~600519~1410.55~1423.36~1423.33~15608~5989~9619~1410.39~3~1410.38~11~1410.18~2~1410.15~1~1410.13~3~1410.55~2~1410.56~3~1410.57~1~1410.59~1~1410.60~2~~20260108120533~-12.81~-0.90~1423.36~1410.13~1410.55/15608/2208731819~15608~220873~0.12~19.62~~1423.36~1410.13~0.93~17663.90~17663.90~7.78~1565.70~1281.02~0.74~11~1415.10~20.50~20.49~~~0.66~220873.1819~0.0000~0~ ~GP-A~2.42~1.50~3.66~35.02~30.58~1606.43~1348.45~0.19~2.43~-1.16~1252270215~1252270215~37.93~-6.37~1252270215~~~1.41~-0.04~~CNY~0~___D__F__N~1410.10~17"; 
	        # 基础信息
        'unknown_1': fields[0],          # 未知字段，通常为1
        'name': fields[1],               # 股票名称
        'code': fields[2],               # 股票代码
        'current_price': fields[3],      # 当前价格
        'pre_close': fields[4],          # 昨日收盘价
        'open': fields[5],               # 今日开盘价
        'volume': fields[6],             # 成交量（手）
        'bid_volume': fields[7],         # 外盘（手）
        'ask_volume': fields[8],         # 内盘（手）
        
        # 实时交易
        'bid1_price': fields[9],         # 买一价
        'bid1_volume': fields[10],       # 买一量（手）
        'bid2_price': fields[11],        # 买二价
        'bid2_volume': fields[12],       # 买二量
        'bid3_price': fields[13],        # 买三价
        'bid3_volume': fields[14],       # 买三量
        'bid4_price': fields[15],        # 买四价
        'bid4_volume': fields[16],       # 买四量
        'bid5_price': fields[17],        # 买五价
        'bid5_volume': fields[18],       # 买五量
        
        'ask1_price': fields[19],        # 卖一价
        'ask1_volume': fields[20],       # 卖一量
        'ask2_price': fields[21],        # 卖二价
        'ask2_volume': fields[22],       # 卖二量
        'ask3_price': fields[23],        # 卖三价
        'ask3_volume': fields[24],       # 卖三量
        'ask4_price': fields[25],        # 卖四价
        'ask4_volume': fields[26],       # 卖四量
        'ask5_price': fields[27],        # 卖五价
        'ask5_volume': fields[28],       # 卖五量
        
        # 行情数据
        'latest_trade': fields[29],      # 最近逐笔成交
        'time': fields[30],              # 时间 HH:MM:SS
        'change_percent': fields[31],    # 涨跌幅%
        'high': fields[32],              # 最高价
        'low': fields[33],               # 最低价
        'price_volume': fields[34],      # 价格/成交量（？）
        'turnover': fields[35],          # 成交额（万元）
        'turnover_rate': fields[36],     # 换手率%
        'pe_ratio': fields[37],          # 市盈率
        'amplitude': fields[38],         # 振幅%
        'market_value': fields[39],      # 流通市值
        'total_market_value': fields[40], # 总市值
        'pb_ratio': fields[45],          # 市净率（第46个字段）
        
        # 扩展字段（如果有）
        'limit_up': fields[46] if len(fields) > 46 else None,  # 涨停价
        'limit_down': fields[47] if len(fields) > 47 else None, # 跌停价
	*/
	int32 startIndex = responseData.Find(TEXT("\"")) + 1;
	int32 endIndex = responseData.Find(TEXT("\";"), ESearchCase::IgnoreCase);
	if (startIndex <= 0 || endIndex <= startIndex) return false;
	FString dataContent = responseData.Mid(startIndex, endIndex - startIndex);
	TArray<FString> dataParts;
	dataContent.ParseIntoArray(dataParts, TEXT("~"), true);
	if (dataParts.Num() >= 40) {
		outRealTimeData.StockName = dataParts[1];
		outRealTimeData.LatestPrice = FCString::Atof(*dataParts[3]);
		outRealTimeData.PreviousClosePrice = FCString::Atof(*dataParts[4]);
		outRealTimeData.ChangeAmount = outRealTimeData.LatestPrice - outRealTimeData.PreviousClosePrice;
		outRealTimeData.ChangeRatio = FCString::Atof(*dataParts[31]);
		outRealTimeData.Volume = FCString::Atof(*dataParts[6]);
		outRealTimeData.Turnover = FCString::Atof(*dataParts[35]);
		outRealTimeData.TurnoverRate = FCString::Atof(*dataParts[36]);
		outRealTimeData.PriceRange = FCString::Atof(*dataParts[38]);
		outRealTimeData.OpenPrice = FCString::Atof(*dataParts[5]);
		outRealTimeData.HighestPrice = FCString::Atof(*dataParts[32]);
		outRealTimeData.LowestPrice = FCString::Atof(*dataParts[33]);
		outRealTimeData.BuyOnePrice = FCString::Atof(*dataParts[9]);
		outRealTimeData.BuyTwoPrice = FCString::Atof(*dataParts[11]);
		outRealTimeData.BuyThreePrice = FCString::Atof(*dataParts[13]);
		outRealTimeData.SellOnePrice = FCString::Atof(*dataParts[19]);
		outRealTimeData.SellTwoPrice = FCString::Atof(*dataParts[21]);
		outRealTimeData.SellThreePrice = FCString::Atof(*dataParts[23]);
		outRealTimeData.PE_Ratio = FCString::Atof(*dataParts[37]);
		outRealTimeData.PB_Ratio = FCString::Atof(*dataParts[45]);
		outRealTimeData.TotalMarketValue = FCString::Atof(*dataParts[40]);
		outRealTimeData.CirculatingMarketValue = FCString::Atof(*dataParts[39]);
		outRealTimeData.UpperLimitPrice = FCString::Atof(*dataParts[46]);
		outRealTimeData.LowerLimitPrice = FCString::Atof(*dataParts[47]);
		outRealTimeData.UpdateTime = dataParts[30];
		return true;
	}
	return false;
}

bool UStockMonitor::ParseTencentResponse(const FString& responseData, TArray<FQTStockRealTimeData>& outRealTimeDatas){
	TArray<FString> stringArray;
	responseData.ParseIntoArray(stringArray, TEXT("\";"));
	if (stringArray.Num() == 0) { UE_LOG(LogTemp, Warning, TEXT("ParseTencentResponse stringArray.Num ==0")); return false; }
	outRealTimeDatas.Empty();
	for (FString& tempStr : stringArray) {
		FString dataContent, leftstr;
		tempStr.Split(TEXT("=\""), &leftstr, &dataContent);
		TArray<FString> dataParts;
		dataContent.ParseIntoArray(dataParts, TEXT("~"));
		FQTStockRealTimeData outRealTimeData;
		if (dataParts.Num() >= 40) {
			outRealTimeData.StockCode = dataParts[2];
			outRealTimeData.StockName = dataParts[1];
			outRealTimeData.LatestPrice = FCString::Atof(*dataParts[3]);
			outRealTimeData.PreviousClosePrice = FCString::Atof(*dataParts[4]);
			outRealTimeData.ChangeAmount = outRealTimeData.LatestPrice - outRealTimeData.PreviousClosePrice;
			outRealTimeData.ChangeRatio = FCString::Atof(*dataParts[31]);
			outRealTimeData.Volume = FCString::Atof(*dataParts[6]);
			outRealTimeData.Turnover = FCString::Atof(*dataParts[35]);
			outRealTimeData.TurnoverRate = FCString::Atof(*dataParts[36]);
			outRealTimeData.PriceRange = FCString::Atof(*dataParts[38]);
			outRealTimeData.OpenPrice = FCString::Atof(*dataParts[5]);
			outRealTimeData.HighestPrice = FCString::Atof(*dataParts[32]);
			outRealTimeData.LowestPrice = FCString::Atof(*dataParts[33]);
			outRealTimeData.BuyOnePrice = FCString::Atof(*dataParts[9]);
			outRealTimeData.BuyTwoPrice = FCString::Atof(*dataParts[11]);
			outRealTimeData.BuyThreePrice = FCString::Atof(*dataParts[13]);
			outRealTimeData.SellOnePrice = FCString::Atof(*dataParts[19]);
			outRealTimeData.SellTwoPrice = FCString::Atof(*dataParts[21]);
			outRealTimeData.SellThreePrice = FCString::Atof(*dataParts[23]);
			outRealTimeData.PE_Ratio = FCString::Atof(*dataParts[37]);
			outRealTimeData.PB_Ratio = FCString::Atof(*dataParts[45]);
			outRealTimeData.TotalMarketValue = FCString::Atof(*dataParts[40]);
			outRealTimeData.CirculatingMarketValue = FCString::Atof(*dataParts[39]);
			outRealTimeData.UpperLimitPrice = FCString::Atof(*dataParts[46]);
			outRealTimeData.LowerLimitPrice = FCString::Atof(*dataParts[47]);
			outRealTimeData.UpdateTime = dataParts[30];
			outRealTimeDatas.Add(outRealTimeData);
		}
	}
	return true;
}

bool UStockMonitor::ParseEMResponse(const FString& responseData, FQTStockRealTimeData& outRealTimeData){
	/*
	{
  "rc": 0,
  "rt": 4,
  "svr": 183640803,
  "lt": 1,
  "full": 1,
  "dlmkts": "",
  "data": {
    "f43": 141230,//最新价
    "f44": 142336,//最高价
    "f45": 140814,//最低价
    "f46": 142333,//今开
    "f47": 29135,//成交量(手)
    "f48": 4117925041,//成交额(元)
    "f49": 11889,//外盘(手)
    "f50": 70,//量比(%)
    "f51": 156570,//涨停价
    "f52": 128102,//跌停价
    "f55": 51.607668966,//每股收益
    "f57": "600519",
    "f58": "贵州茅台",
    "f59": 2,
    "f60": 142336,//昨日收盘价
    "f62": 3,
    "f71": 141342,//均价
    "f78": 0,
    "f80": "[{\"b\":202601080930,\"e\":202601081130},{\"b\":202601081300,\"e\":202601081500}]",//日期
    "f84": 1252270215,//总股本
    "f85": 1252270215,//流通股本
    "f86": 1767859893,
    "f92": 181.326142,//每股净资产
    "f103": 86239108106.25,
    "f104": 181925416967.68,
    "f105": 64626746712.18,
    "f106": 100,
    "f107": 1,
    "f108": 71.891305836,
    "f109": 86228146421.62,
    "f110": 1,
    "f111": 2,
    "f112": "2",
    "f113": 0,
    "f114": 0,
    "f115": 0,
    "f116": 1768581224644.5,//总市值(元)
    "f117": 1768581224644.5,//流通市值(元)
    "f31": 141260,//卖五价
    "f32": 3,
    "f33": 141259,//卖四价
    "f34": 1,
    "f35": 141236,//卖三价
    "f36": 3,
    "f37": 141235,//卖二价
    "f38": 1,
    "f39": 141232,//卖一价
    "f40": 1,
    "f19": 141230,//买一价
    "f20": 3,
    "f17": 141229,//买二价
    "f18": 2,
    "f15": 141228,//买三价
    "f16": 3,
    "f13": 141227,//买四价
    "f14": 26,
    "f11": 141222,//买五价
    "f12": 8
	  }
	}
	*/
	TSharedPtr<FJsonObject> jsonObject;
	TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(responseData);
	if (FJsonSerializer::Deserialize(reader, jsonObject) && jsonObject.IsValid()) {
		TSharedPtr<FJsonObject> dataObject = jsonObject->GetObjectField(TEXT("data"));
		if (dataObject.IsValid()) {
			outRealTimeData.StockName = dataObject->GetStringField(TEXT("f58"));
			outRealTimeData.LatestPrice = dataObject->GetNumberField(TEXT("f43")) / 100.0f;
			outRealTimeData.PreviousClosePrice = dataObject->GetNumberField(TEXT("f60")) / 100.0f;
			outRealTimeData.ChangeAmount = outRealTimeData.LatestPrice - outRealTimeData.PreviousClosePrice;
			outRealTimeData.ChangeRatio = outRealTimeData.ChangeAmount / outRealTimeData.PreviousClosePrice * 100.0f;
			outRealTimeData.Volume = dataObject->GetNumberField(TEXT("f47")) / 10000; // 转换为万手
			outRealTimeData.Turnover = dataObject->GetNumberField(TEXT("f48")) / 100000000.0f; // 转换为亿元
			outRealTimeData.TurnoverRate = dataObject->GetNumberField(TEXT("f47")) / dataObject->GetNumberField(TEXT("f85")) * 10000.0f; // 换手率(%)(每手100股)
			outRealTimeData.VolumeRatio = dataObject->GetNumberField(TEXT("f50"));
			outRealTimeData.OpenPrice = dataObject->GetNumberField(TEXT("f46")) / 100.0f;
			outRealTimeData.HighestPrice = dataObject->GetNumberField(TEXT("f44")) / 100.0f;
			outRealTimeData.LowestPrice = dataObject->GetNumberField(TEXT("f45")) / 100.0f;
			outRealTimeData.PriceRange = (outRealTimeData.HighestPrice - outRealTimeData.LowestPrice) / outRealTimeData.PreviousClosePrice * 100.0f;
			outRealTimeData.UpperLimitPrice = dataObject->GetNumberField(TEXT("f51")) / 100.0f;
			outRealTimeData.LowerLimitPrice = dataObject->GetNumberField(TEXT("f52")) / 100.0f;
			outRealTimeData.BuyOnePrice = dataObject->GetNumberField(TEXT("f19")) / 100.0f;
			outRealTimeData.BuyTwoPrice = dataObject->GetNumberField(TEXT("f17")) / 100.0f;
			outRealTimeData.BuyThreePrice = dataObject->GetNumberField(TEXT("f15")) / 100.0f;
			outRealTimeData.SellOnePrice = dataObject->GetNumberField(TEXT("f39")) / 100.0f;
			outRealTimeData.SellTwoPrice = dataObject->GetNumberField(TEXT("f37")) / 100.0f;
			outRealTimeData.SellThreePrice = dataObject->GetNumberField(TEXT("f35")) / 100.0f;
			outRealTimeData.PE_Ratio = outRealTimeData.LatestPrice / dataObject->GetNumberField(TEXT("f55"));
			outRealTimeData.PB_Ratio = outRealTimeData.LatestPrice / dataObject->GetNumberField(TEXT("f92"));
			outRealTimeData.TotalMarketValue = dataObject->GetNumberField(TEXT("f116")) / 100000000.0f; // 转换为亿元
			outRealTimeData.CirculatingMarketValue = dataObject->GetNumberField(TEXT("f117")) / 100000000.0f; // 转换为亿元
			outRealTimeData.UpdateTime = FDateTime::Now().ToString(TEXT("%H:%M:%S"));
			return true;
		}
	}
	return false;
}

bool UStockMonitor::ParseF10FinanceMainResponse(const FString& responseData, FQTFinancialF10Main& outRealTimeData){
	// 解析东方财富F10财务数据
	/*
	//资产负债表和利润表数据格式
	{
	"ORG_CODE":"10002602",
	"SECURITY_CODE":"600519", //股票代码
	"SECUCODE":"600519.SH", 
	"SECURITY_NAME_ABBR":"贵州茅台",    //股票简称
	"SECURITY_TYPE_CODE":"058001001",   //类型代码
	"REPORT_TYPE":"2025三季报", //报告类型
	"REPORT_DATE_NAME":"三季报", //报告日期名称
	"ORG_TYPE":"通用",
	"TOTAL_OPERATE_INCOME":130903889634.88, //营业总收入
	"TOTAL_ASSETS":304738184929.86, //总资产
	"TOTAL_LIABILITIES":39033160057.01, //总负债
	"NETPROFIT":66898804746.17, //净利润
	"OTHER_INCOME":24453342.83, //其它收益
	"INVEST_INCOME":59165.27,   //投资收益
	"FAIRVALUE_CHANGE_INCOME":6956011.89,   //公允价值变动收益
	"ASSET_DISPOSAL_INCOME":511925.45,  //资产处置收益
	"REPORT_DATE":"2025-09-30 00:00:00",    //报告时间
	"NOTICE_DATE":"2025-10-30 00:00:00",    //通告时间
	"UPDATE_DATE":"2025-10-30 00:00:00",    //更新时间
	"CURRENCY":"CNY",   //货币类型人民币
	"OPERATE_COST":11183972073.77,  //营业成本
	"SALE_EXPENSE":4478672252.48,   //销售费用
	"MANAGE_EXPENSE":5503383668.62, //管理费用
	"RESEARCH_EXPENSE":113086640.98, //研发费用
	"FINANCE_EXPENSE":-634730437.77,    //财务费用
	"OPERATE_TAX_ADD":20645707926.55,   //营业税金及附加
	"INCOME_TAX":22504639708.51,    //所得税
	"CREDIT_IMPAIRMENT_INCOME":-20021460.99,    //信贷减值收益
	"ASSET_IMPAIRMENT_INCOME":null, //资产减值收益
	"NONBUSINESS_EXPENSE":126220158.04, //营业外支出
	"NONBUSINESS_INCOME":39970046.24,   //营业外收入
	"OPERATE_PROFIT":89489694566.48,    //营业利润
	"TOTAL_PROFIT":89403444454.68,  //利润总额
	"TOTAL_CURRENT_LIAB":38763379268.53,    //流动负债合计
	"TOTAL_NONCURRENT_LIAB":269780788.48,   //非流动负债合计
	"TOTAL_EXPENSE":9460412124.31,  //期间总费用(元)
	"TOTAL_CURRENT_ASSETS":256587161700.86, //流动资产合计
	"TOTAL_NONCURRENT_ASSETS":48151023229,  //非流动资产合计
	"MONETARYFUNDS":51753057846.45, //货币资金
	"TRADE_FINASSET":null,  //交易金融资产
	"ACCOUNTS_RECE":25531737.62,    //应收账款
	"PREPAYMENT":21229757.91,   //预付款项
	"INVENTORY":55858862716.48, //存货
	"FIXED_ASSET":21170758112.81,   //固定资产
	"INTANGIBLE_ASSET":8649225375.62,   //无形资产
	"LONG_PREPAID_EXPENSE":137287075.5, //长期待摊费用
	"NOTE_RECE":5209529939.88,  //应收票据
	"FINANCE_RECE":null,    //财务收据
	"LONG_RECE":null,   //
	"LONG_EQUITY_INVEST":null,  //长期股权投资
	"OTHER_EQUITY_INVEST":null, //其他股权投资
	"INVEST_REALESTATE":3818474.9,  //投资房地产
	"CIP":3534669471.18,    //在建工程
	"USERIGHT_ASSET":228658932.68,  //使用权资产
	"GOODWILL":null,    //商誉
	"DERIVE_FINLIAB":null,  //导出最终负债
	"SHORT_LOAN":null,  //短期贷款
	"NOTE_PAYABLE":null,    //应付票据
	"ACCOUNTS_PAYABLE":2822271882.72,   //应付票据及应付账款
	"CONTRACT_LIAB":7749027043.43,  //合同负债
	"STAFF_SALARY_PAYABLE":471949375.75,    //应付职工薪酬
	"LONG_LOAN":null,   //长期贷款
	"LEASE_LIAB":205116273.86,  //租赁负债
	"LONG_PAYABLE":null,    //长期应付
	"TOTAL_OPERATE_COST":41426154052.85,    //总运营成本
	"INTERFACE_TYPE":1, //接口类型
	"NOTE_ACCOUNTS_PAYABLE":2822271882.72,  //应付账款说明
	"OTHER_PAYABLE":5374646780.18   //其他应付款合计
	"EPSJB":51.53,  //基本每股收益
	"EPSKCJB":null, //扣非每股收益
	"EPSXS":51.53,  //稀释每股收益
	"BPS":205.283142014385, //每股净资产
	"MGZBGJ":1.097977416735, //每股公积金
	"MGWFPLR":168.39417445809,  //每股未分配利润
	"MGJYXJJE":30.502044764572, //每股经营现金流
	"TOTALOPERATEREVE":130903889634.88, //营业总收入
	"MLR":117269735582.09,  //毛利润
	"PARENTNETPROFIT":64626746712.18,   //归属净利润
	"KCFJCXSYJLR":64680616431.2,    //扣非净利润
	"TOTALOPERATEREVETZ":6.3200018806,  //营业总收入同比增长(%)
	"PARENTNETPROFITTZ":6.2458449524,   //归属净利润同比增长(%)
	"KCFJCXSYJLRTZ":6.4199772865,   //扣非净利润同比增长(%)
	"YYZSRGDHBZC":0.076325235299,   //营业总收入滚动环比增长(%)
	"NETPROFITRPHBZC":0.102121277195,   //归属净利润滚动环比增长(%)
	"KFJLRGDHBZC":0.202115427649,   //扣非净利润滚动环比增长(%)
	"ROEJQ":24.64,  //净资产收益率(加权)(%)
	"ROEKCJQ":null, //净资产收益率(扣非/加权)(%)
	"ZZCJLL":22.1635629313, //总资产收益率(加权)(%)
	"XSJLL":52.0800885914,  //净利率(%)
	"XSMLL":91.293383213404,    //毛利率(%)
	"YSZKYYSR":null,    //预收账款/营业收入
	"XSJXLYYSR":1.013567024339, //销售净现金流/营业收入
	"JYXJLYYSR":0.297358502548, //经营净现金流/营业收入
	"TAXRATE":25.1720052239,    //实际税率(%)
	"LD":6.619318711183,    //流动比率
	"SD":5.178297216913,    //速动比率
	"XJLLB":0.985383701732, //现金流量比率
	"ZCFZL":12.8087525579,  //资产负债率(%)
	"QYCS":1.146904109457,  //权益系数
	"CQBL":0.146904109456,  //产权比率
	"ZZCZZTS":622.572587277261, //总资产周转天数(天)
	"CHZZTS":1330.23311081961,  //存货周转天数(天)
	"YSZKZZTS":0.046774053545,  //应收账款周转天数(天)
	"TOAZZL":0.43368436953, //总资产周转率(次)
	"CHZZL":0.202971943642, //存货周转率(次)
	"YSZKZZL":5772.43107100381, //应收账款周转率(次)
	}
	*/
	TSharedPtr<FJsonObject> jsonObject;
	TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(responseData);
	if (FJsonSerializer::Deserialize(reader, jsonObject) && jsonObject.IsValid()) {
		TSharedPtr<FJsonObject> resultObject = jsonObject->GetObjectField(TEXT("result"));
		if (!resultObject.IsValid())return false;
		const TArray<TSharedPtr<FJsonValue>>* dataArray;
		if (resultObject->TryGetArrayField(TEXT("data"), dataArray)) {
			TSharedPtr<FJsonObject> dataObject = (*dataArray)[0]->AsObject();
			if (!dataObject.IsValid()) return false;
			if (dataObject->HasField(TEXT("SECURITY_CODE")))outRealTimeData.SECURITY_CODE = dataObject->GetStringField(TEXT("SECURITY_CODE"));
			if (dataObject->HasField(TEXT("SECURITY_NAME_ABBR")))outRealTimeData.SECURITY_NAME_ABBR = dataObject->GetStringField(TEXT("SECURITY_NAME_ABBR"));
			if (dataObject->HasField(TEXT("REPORT_TYPE")))outRealTimeData.REPORT_TYPE = dataObject->GetStringField(TEXT("REPORT_TYPE"));
			if (dataObject->HasField(TEXT("REPORT_DATE")))outRealTimeData.REPORT_DATE = dataObject->GetStringField(TEXT("REPORT_DATE"));
			if (dataObject->HasField(TEXT("TOTAL_ASSETS")))outRealTimeData.TOTAL_ASSETS = dataObject->GetNumberField(TEXT("TOTAL_ASSETS"));
			if (dataObject->HasField(TEXT("TOTAL_PARENT_EQUITY")))outRealTimeData.TOTAL_PARENT_EQUITY = dataObject->GetNumberField(TEXT("TOTAL_PARENT_EQUITY"));
			if (dataObject->HasField(TEXT("TOTAL_CURRENT_ASSETS")))outRealTimeData.TOTAL_CURRENT_ASSETS = dataObject->GetNumberField(TEXT("TOTAL_CURRENT_ASSETS"));
			if (dataObject->HasField(TEXT("TOTAL_NONCURRENT_ASSETS")))outRealTimeData.TOTAL_NONCURRENT_ASSETS = dataObject->GetNumberField(TEXT("TOTAL_NONCURRENT_ASSETS"));
			if (dataObject->HasField(TEXT("TOTAL_LIABILITIES")))outRealTimeData.TOTAL_LIABILITIES = dataObject->GetNumberField(TEXT("TOTAL_LIABILITIES"));
			if (dataObject->HasField(TEXT("TOTAL_OPERATE_COST")))outRealTimeData.TOTAL_OPERATE_COST = dataObject->GetNumberField(TEXT("TOTAL_OPERATE_COST"));
			if (dataObject->HasField(TEXT("TOTAL_OPERATE_INCOME")))outRealTimeData.TOTAL_OPERATE_INCOME = dataObject->GetNumberField(TEXT("TOTAL_OPERATE_INCOME"));
			if (dataObject->HasField(TEXT("EPSJB")))outRealTimeData.EPSJB = dataObject->GetNumberField(TEXT("EPSJB"));
			if (dataObject->HasField(TEXT("EPSXS")))outRealTimeData.EPSXS = dataObject->GetNumberField(TEXT("EPSXS"));
			if (dataObject->HasField(TEXT("BPS")))outRealTimeData.BPS = dataObject->GetNumberField(TEXT("BPS"));
			if (dataObject->HasField(TEXT("MGJYXJJE")))outRealTimeData.MGJYXJJE = dataObject->GetNumberField(TEXT("MGJYXJJE"));
			if (dataObject->HasField(TEXT("MLR")))outRealTimeData.MLR = dataObject->GetNumberField(TEXT("MLR"));
			if (dataObject->HasField(TEXT("PARENTNETPROFIT")))outRealTimeData.PARENTNETPROFIT = dataObject->GetNumberField(TEXT("PARENTNETPROFIT"));
			if (dataObject->HasField(TEXT("ROEJQ")))outRealTimeData.ROEJQ = dataObject->GetNumberField(TEXT("ROEJQ"));
			if (dataObject->HasField(TEXT("ZZCJLL")))outRealTimeData.ZZCJLL = dataObject->GetNumberField(TEXT("ZZCJLL"));
			if (dataObject->HasField(TEXT("XSMLL")))outRealTimeData.XSMLL = dataObject->GetNumberField(TEXT("XSMLL"));
			if (dataObject->HasField(TEXT("XSJLL")))outRealTimeData.XSJLL = dataObject->GetNumberField(TEXT("XSJLL"));
			if (dataObject->HasField(TEXT("ZCFZL")))outRealTimeData.ZCFZL = dataObject->GetNumberField(TEXT("ZCFZL"));
			return true;
		}
	}
	return false;
}
