#include "RecommendStocksWidget.h"
#include "QTCurveVectorActor.h"
#include "StockMonitor.h"
#include "CompanyNameIndexWidget.h"

bool URecommendStocksWidget::LoadStocksFromRecentStockListJson(const TArray<FString>& filterChars){
	FString stockListFilename = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/RecentStockList.json"));
	FString fileContent;
	bool loadsuccesful = FFileHelper::LoadFileToString(fileContent, *stockListFilename);
	if (loadsuccesful) {
		TSharedPtr<FJsonObject> jsonObject;
		TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(fileContent);
		if (FJsonSerializer::Deserialize(jsonReader, jsonObject) && jsonObject.IsValid()) {
			const TArray<TSharedPtr<FJsonValue>>* stockListArray;//股票
			if (jsonObject->TryGetArrayField(TEXT("StockList"), stockListArray)) {
				if (stockListArray->IsEmpty()) { UE_LOG(LogTemp, Error, TEXT("---------->> stockListArray数据为空!")); return false; }
				for (auto& eachStockValue : *stockListArray) {
					TSharedPtr<FJsonObject> stockObject = eachStockValue->AsObject();
					if (stockObject.IsValid()) {
						FString stockCode, stockName;
						stockObject->TryGetStringField(TEXT("CODE"), stockCode);
						stockObject->TryGetStringField(TEXT("NAME"), stockName);
						if (stockName.StartsWith(filterChars[0]) || stockName.StartsWith(filterChars[1])) continue;//如果股票名称以过滤字符开头,就跳过这个股票
						RecommendStocks_.Add(stockCode, 0);//默认权重值是0,后续根据财务数据分析结果调整权重值,权重值小于0的相当于直接筛掉了
					}
				}
				return true;
			}
			else {UE_LOG(LogTemp, Error, TEXT("---------->> StockList数据字段缺失!")); return false;}
		}
		else { UE_LOG(LogTemp, Error, TEXT("---------->> 解析股票列表数据文件失败!")); return false; }
	}
	else { UE_LOG(LogTemp, Error, TEXT("---------->> 加载股票列表数据文件失败!")); return false; }
	return false;
}

bool URecommendStocksWidget::LoadStocksForSellFromJson(FString jsonFileName){
	FString stockListFilename = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/%s"), *jsonFileName);
	FString fileContent;
	bool loadsuccesful = FFileHelper::LoadFileToString(fileContent, *stockListFilename);
	if (loadsuccesful) {
		TSharedPtr<FJsonObject> jsonObject;
		TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(fileContent);
		if (FJsonSerializer::Deserialize(jsonReader, jsonObject) && jsonObject.IsValid()) {
			const TArray<TSharedPtr<FJsonValue>>* stockListArray;//股票
			if (jsonObject->TryGetArrayField(TEXT("StockList"), stockListArray)) {
				if (stockListArray->IsEmpty()) { UE_LOG(LogTemp, Error, TEXT("---------->> %s stockListArray数据为空!"), *stockListFilename); return false; }
				for (auto& eachStockValue : *stockListArray) {
					TSharedPtr<FJsonObject> stockObject = eachStockValue->AsObject();
					if (stockObject.IsValid()) {
						FString stockCode;
						stockObject->TryGetStringField(TEXT("CODE"), stockCode);
						RecommendSellStocks_.Add(stockCode, 0.0f);
					}
				}
				return true;
			}
			else { UE_LOG(LogTemp, Error, TEXT("---------->>%s StockList数据字段缺失!"),*stockListFilename); return false; }
		}
		else { UE_LOG(LogTemp, Error, TEXT("---------->>%s 解析股票列表数据文件失败!"), *stockListFilename); return false; }
	}
	else { UE_LOG(LogTemp, Error, TEXT("---------->>%s 加载股票列表数据文件失败!"), *stockListFilename); return false; }
	return false;
}

void URecommendStocksWidget::StartFilterStocks(){
	progress = 0.0f;
	bool success = false;
	success = LoadStocksFromRecentStockListJson({ TEXT("*ST"),TEXT("ST") });//首先筛选掉所有*ST和ST开头的股票,因为这些股票通常是有退市风险的,不适合推荐给用户
	if (success) {//然后对RecommendStocks_里面的股票逐一进行财务基本面分析,ROE<0的一律筛掉,ROE越大越优先推荐,其次再检查资产负债率,大于80%的筛掉,小于60%的优先.最后再检查现金流,小于0的筛掉,每股现金流>每股净收益的优先考虑;
		TArray<FString> allStocks;
		RecommendStocks_.GenerateKeyArray(allStocks);
		UpdateStockKLineF10(allStocks);//首先触发更新K线和财务的函数,那边会相隔不固定的时间段逐一更新RecommendStocks_里面的所有股票K线数据和F10数据;更新完之后会触发财指标分析函数
	}
}

void URecommendStocksWidget::StartFilterSellStocks(){
	LoadStocksForSellFromJson(TEXT("HoldingStockList.json"));
	LoadStocksForSellFromJson(TEXT("OutingStockList.json"));
	LoadStocksForSellFromJson(TEXT("WaitingOutStockList.json"));
	TArray<FString> allStocks;
	RecommendSellStocks_.GenerateKeyArray(allStocks);
	UpdateStockKLine(allStocks,1);//首先触发更新K线和财务的函数,那边会相隔不固定的时间段逐一更新RecommendStocks_里面的所有股票K线数据和F10数据;更新完之后会触发财指标分析函数
}

TArray<EIndicatorColor> URecommendStocksWidget::GetStockIndicatorColors(FString stockCode){
	if (StockIndicatorColors_.Contains(stockCode))	return StockIndicatorColors_[stockCode];
	return TArray<EIndicatorColor>();
}

void URecommendStocksWidget::NativeConstruct(){
	Super::NativeConstruct();
	stockMonitor_ = NewObject<UStockMonitor>(this);
	if (stockMonitor_) {
		stockMonitor_->SetTimeOut(10.0f);
		stockMonitor_->SetRetryCount(3);
		stockMonitor_->SetCallbacks(
			FOnRequestSuccess::CreateUObject(this, &URecommendStocksWidget::HandleStockDataResponse),
			FOnRequestSuccessF10::CreateUObject(this, &URecommendStocksWidget::HandleF10DataResponse),
			FOnRequestFailed::CreateUObject(this, &URecommendStocksWidget::HandleStockDataError)
		);
	}
}

void URecommendStocksWidget::HandleStockDataResponse(const FString& ResponseData, int32 insource) {
	//存放推荐的前10只股票最新实时数据的数组
	TArray< FQTStockRealTimeData> realTimeDatas;
	bool bParseSuccess = false;
	if (insource == 0) { UE_LOG(LogTemp, Warning, TEXT("---------->> 东方财富网暂时没有实现同时获取多只股票实时数据!")); }
	else if (insource == 1) bParseSuccess = stockMonitor_->ParseTencentResponse(ResponseData, realTimeDatas);
	else if (insource == 2) { UE_LOG(LogTemp, Warning, TEXT("---------->> 新浪财经网暂时没有实现同时获取多只股票实时数据!")); }

	if (bParseSuccess) {//解析成功,把数据存储到成员变量里,然后触发显示推荐股票的函数
		UE_LOG(LogTemp, Warning, TEXT("---------->> 前10只推荐股票实时数据解析成功!"));
		DisplayRecommendedStocks(realTimeDatas);
	}
	else { UE_LOG(LogTemp, Warning, TEXT("---------->> 解析股票实时数据失败: %s"), *ResponseData); }
}
void URecommendStocksWidget::HandleF10DataResponse(const FString& responseData, int32 insource) {
	TArray<FQTFinancialF10Main> F10Datas_;
	bool bParseSuccess = ParseF10sFinanceMainResponse(responseData, F10Datas_);
	if (bParseSuccess) {
		//然后把F10数据存储到json文件
		TSharedPtr<FJsonObject> jsonObjectForWrite = MakeShareable(new FJsonObject());
		jsonObjectForWrite->SetNumberField(TEXT("FetchedAt"), FDateTime::Now().GetYear() * 10000 + FDateTime::Now().GetMonth() * 100 + FDateTime::Now().GetDay());
		TArray<TSharedPtr<FJsonValue>> f10DataArray;
		for (const auto& tempF10Data : F10Datas_) {
			TSharedPtr<FJsonObject> tempObject = MakeShareable(new FJsonObject());
			tempObject->SetStringField(TEXT("SECURITY_CODE"), tempF10Data.SECURITY_CODE );
			tempObject->SetStringField(TEXT("SECURITY_NAME_ABBR"), tempF10Data.SECURITY_NAME_ABBR );
			tempObject->SetStringField(TEXT("REPORT_TYPE"), tempF10Data.REPORT_TYPE );
			FString reportDate = tempF10Data.REPORT_DATE;
			tempObject->SetStringField(TEXT("REPORT_DATE"), reportDate);
			FString yearStr = reportDate.Left(4);
			FString mouthStr = reportDate.Mid(5).Left(2);
			FString dayStr = reportDate.Mid(8).Left(2);
			tempObject->SetNumberField(TEXT("REPORTDATE"), 10000 * FCString::Atoi(*yearStr) + 100 * FCString::Atoi(*mouthStr) + FCString::Atoi(*dayStr));
			tempObject->SetNumberField(TEXT("TOTAL_ASSETS"), tempF10Data.TOTAL_ASSETS );
			tempObject->SetNumberField(TEXT("TOTAL_PARENT_EQUITY"), tempF10Data.TOTAL_PARENT_EQUITY );
			tempObject->SetNumberField(TEXT("TOTAL_CURRENT_ASSETS"), tempF10Data.TOTAL_CURRENT_ASSETS);
			tempObject->SetNumberField(TEXT("TOTAL_NONCURRENT_ASSETS"), tempF10Data.TOTAL_NONCURRENT_ASSETS );
			tempObject->SetNumberField(TEXT("TOTAL_LIABILITIES"), tempF10Data.TOTAL_LIABILITIES );
			tempObject->SetNumberField(TEXT("TOTAL_OPERATE_COST"), tempF10Data.TOTAL_OPERATE_COST );
			tempObject->SetNumberField(TEXT("TOTAL_OPERATE_INCOME"), tempF10Data.TOTAL_OPERATE_INCOME );
			tempObject->SetNumberField(TEXT("EPSJB"), tempF10Data.EPSJB );
			tempObject->SetNumberField(TEXT("EPSXS"), tempF10Data.EPSXS );
			tempObject->SetNumberField(TEXT("BPS"), tempF10Data.BPS );
			tempObject->SetNumberField(TEXT("MGJYXJJE"), tempF10Data.MGJYXJJE );
			tempObject->SetNumberField(TEXT("MLR"), tempF10Data.MLR );
			tempObject->SetNumberField(TEXT("PARENTNETPROFIT"), tempF10Data.PARENTNETPROFIT );
			tempObject->SetNumberField(TEXT("ROEJQ"), tempF10Data.ROEJQ );
			tempObject->SetNumberField(TEXT("ZZCJLL"), tempF10Data.ZZCJLL );
			tempObject->SetNumberField(TEXT("XSMLL"), tempF10Data.XSMLL );
			tempObject->SetNumberField(TEXT("XSJLL"), tempF10Data.XSJLL );
			tempObject->SetNumberField(TEXT("ZCFZL"), tempF10Data.ZCFZL );
			f10DataArray.Add(MakeShareable(new FJsonValueObject(tempObject)));
		}
		jsonObjectForWrite->SetArrayField(TEXT("F10"), f10DataArray);
		FString outputString;
		TSharedRef<TJsonWriter<>> jsonWriter = TJsonWriterFactory<>::Create(&outputString);
		if (FJsonSerializer::Serialize(jsonObjectForWrite.ToSharedRef(), jsonWriter)) {
			TSharedPtr<FQTStockListRow>  tempStockListRow = companyNameIndexWidgetBP->GetFQTStockListRowByCodeOrName(F10Datas_[0].SECURITY_CODE);
			FString f10Path= FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/KlineDatas/%s/F10.json"), *(tempStockListRow->NAMECODE));
			if (FFileHelper::SaveStringToFile(outputString, *f10Path)) {
				UE_LOG(LogTemp, Warning, TEXT("---------->> 股票F10数据已成功保存到本地文件: %s"), *f10Path);
				PlusGetF10Counts();
			}
			else UE_LOG(LogTemp, Error, TEXT("---------->> 保存股票F10数据到本地文件失败: %s"), *f10Path);
		}
		else UE_LOG(LogTemp, Error, TEXT("---------->> 序列化股票F10数据失败!"));
	}
	else 	UE_LOG(LogTemp, Warning, TEXT("---------------------------->> 解析历史F10财务数据失败: %s"), *responseData);
}
void URecommendStocksWidget::HandleStockDataError(int32 ErrorCode, const FString& ErrorMessage) {
	UE_LOG(LogTemp, Error, TEXT("股票数据请求错误: %d - %s"), ErrorCode, *ErrorMessage);
}

bool URecommendStocksWidget::ParseF10sFinanceMainResponse(const FString& responseData, TArray<FQTFinancialF10Main>& outF10s) {
	TSharedPtr<FJsonObject> jsonObject;
	TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(responseData);
	if (FJsonSerializer::Deserialize(jsonReader, jsonObject) && jsonObject.IsValid()) {
		TSharedPtr<FJsonObject> resultObject = jsonObject->GetObjectField(TEXT("result"));
		if (!resultObject.IsValid()) { UE_LOG(LogTemp, Warning, TEXT("解析F10财务数据时缺少result字段!")); return false; }
		const TArray<TSharedPtr<FJsonValue>>* f10DataArray;//财务数据列表
		if (resultObject->TryGetArrayField(TEXT("data"), f10DataArray)) {
			outF10s.Empty();
			for (auto& eachF10Value : *f10DataArray) {
				TSharedPtr<FJsonObject> dataObject = eachF10Value->AsObject();
				if (dataObject.IsValid()) {
					FQTFinancialF10Main outRealTimeData;
					if (dataObject->HasField(TEXT("SECURITY_CODE")))outRealTimeData.SECURITY_CODE = dataObject->GetStringField(TEXT("SECURITY_CODE"));
					if (dataObject->HasField(TEXT("SECURITY_NAME_ABBR")))outRealTimeData.SECURITY_NAME_ABBR = dataObject->GetStringField(TEXT("SECURITY_NAME_ABBR"));
					if (dataObject->HasField(TEXT("REPORT_TYPE")))outRealTimeData.REPORT_TYPE = dataObject->GetStringField(TEXT("REPORT_TYPE"));
					if (dataObject->HasField(TEXT("REPORT_DATE")))outRealTimeData.REPORT_DATE = dataObject->GetStringField(TEXT("REPORT_DATE"));
					if (dataObject->HasField(TEXT("REPORTDATE")))outRealTimeData.REPORTDATE = dataObject->GetNumberField(TEXT("REPORTDATE"));
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
					outF10s.Add(outRealTimeData);
				}
				else { UE_LOG(LogTemp, Warning, TEXT("解析F10财务数据时遇到无效数据对象!")); }
			}
			return true;
		}
		else { UE_LOG(LogTemp, Warning, TEXT("解析F10财务数据时缺少data字段!")); return false; }
	}
	return false;
}

void URecommendStocksWidget::GetKLineDatasByStockCode(FString stockCode) {
	progress = (GetKLineCounts + GetF10Counts) / (2.0f * RecommendStocks_.Num());
	UE_LOG(LogTemp, Warning, TEXT("---------->>  GetKLineDatasByStockCode::正在从网站获取%s的K线数据 JustSave"), *GetNameCode(stockCode));
	companyNameIndexWidgetBP->FetchKLineDataJustSave(stockCode);
}

bool URecommendStocksWidget::NeedToDownLoadKLineFromInternet(FString stockCode) {
	TSharedPtr<FJsonObject> jsonObject;
	if (!LoadLocalKLineData(stockCode, jsonObject)) { return true; }//如果加载失败就需要重新请求网络HTTP获取数据
	int fetchedTime;
	jsonObject->TryGetNumberField(TEXT("FetchedAt"), fetchedTime);
	int currentTime = FDateTime::Now().GetYear() * 10000 + FDateTime::Now().GetMonth() * 100 + FDateTime::Now().GetDay();
	EDayOfWeek weekday = FDateTime::Now().GetDayOfWeek();
	if (weekday == EDayOfWeek::Saturday)currentTime -= 1;//如果是周六就往前推1天
	if (weekday == EDayOfWeek::Sunday)currentTime -= 2;//如果是周日就往前推2天
	if (fetchedTime < currentTime) { return true; }//如果数据超过一天没更新,就需要重新请求网络HTTP获取数据
	return false;
}

bool URecommendStocksWidget::LoadLocalKLineData(FString stockCode, TSharedPtr<FJsonObject>& outJsonObj){
	FString fileContent;
	FString klinefilepath = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/KlineDatas/%s/Kline101.json"), *GetNameCode(stockCode));
	bool loadsuccesful = FFileHelper::LoadFileToString(fileContent, *klinefilepath);
	if (!loadsuccesful) {//如果加载失败返回false
		return false;
	}
	TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(fileContent);
	return FJsonSerializer::Deserialize(jsonReader, outJsonObj);
}

void URecommendStocksWidget::GetF10DatasByStockCode(FString stockCode){
	if (!IsValid(stockMonitor_)) stockMonitor_ = NewObject<UStockMonitor>(this);
	if (!IsValid(stockMonitor_)) {
		UE_LOG(LogTemp, Error, TEXT("UStockMonitor is invalid, cannot fetch F10 data for %s"), *GetNameCode(stockCode));
		return;
	}
	// 防止除零错误
	int32 StockCount = FMath::Max(RecommendStocks_.Num(), 1);
	progress = (GetKLineCounts + GetF10Counts) / (2.0f * StockCount);
	UE_LOG(LogTemp, Warning, TEXT("---------->>  GetF10DatasByStockCode::正在从网站获取%s的F10数据 JustSave --Progress %f"), *GetNameCode(stockCode), progress);
	stockMonitor_->GetStockF10FianceMainDatas(stockCode);
}

bool URecommendStocksWidget::NeedToDownLoadF10FromInternet(FString stockCode){
	TSharedPtr<FJsonObject> jsonObject;
	if(!LoadLocalF10Data(stockCode, jsonObject)) { return true; }//如果加载失败就需要重新请求网络HTTP获取数据
	int fetchedTime;
	jsonObject->TryGetNumberField(TEXT("FetchedAt"), fetchedTime);
	int currentTime = FDateTime::Now().GetYear() * 10000 + FDateTime::Now().GetMonth() * 100 + FDateTime::Now().GetDay();
	EDayOfWeek weekday = FDateTime::Now().GetDayOfWeek();
	if (weekday == EDayOfWeek::Saturday)currentTime -= 1;//如果是周六就往前推1天
	if (weekday == EDayOfWeek::Sunday)currentTime -= 2;//如果是周日就往前推2天
	if (fetchedTime < currentTime) { return true; }//如果数据超过一天没更新,就需要重新请求网络HTTP获取数据
	return false;
}

bool URecommendStocksWidget::LoadLocalF10Data(FString stockCode, TSharedPtr<FJsonObject>& outJsonObj){
	FString fileContent;
	FString f10filepath = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/KlineDatas/%s/F10.json"), *GetNameCode(stockCode));
	bool loadsuccesful = FFileHelper::LoadFileToString(fileContent, *f10filepath);
	if (!loadsuccesful) {//如果加载失败返回false
		return false;
	}
	TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(fileContent);
	return FJsonSerializer::Deserialize(jsonReader, outJsonObj);
}

FString URecommendStocksWidget::GetNameCode(FString stockCode){
	TSharedPtr<FQTStockListRow>  tempStockListRow = companyNameIndexWidgetBP->GetFQTStockListRowByCodeOrName(stockCode);//根据股票代码获取对应的本地文件路径
	return tempStockListRow->NAMECODE;
}

void URecommendStocksWidget::PlusGetKLineCounts(int buyOrSell){
	GetKLineCounts++;
	if (buyOrSell == 0) {
		UE_LOG(LogTemp, Warning, TEXT("------------>> 股票K线更新个数:%d"), GetKLineCounts);
		if (GetKLineCounts == RecommendStocks_.Num()) {//当股票K线刷新完开始执行市盈率分析
			UE_LOG(LogTemp, Warning, TEXT("----------------->> 股票K线更新完毕!!!"));
			if (GetF10Counts == RecommendStocks_.Num()) {
				UE_LOG(LogTemp, Warning, TEXT("----------------->> 股票的F10财务数据也已经更新完毕,开始进行财务分析!!!"));
				//开始分析财务指标数据
				TArray<FString> allStocks;
				RecommendStocks_.GenerateKeyArray(allStocks);
				AnalyzeStockDatasForBuy(allStocks);
			}
		}
	}
	else if (buyOrSell == 1) {
		UE_LOG(LogTemp, Warning, TEXT("------------>> 卖出股票K线更新个数:%d"), GetKLineCounts);
		if (GetKLineCounts == RecommendSellStocks_.Num()) {
			UE_LOG(LogTemp, Warning, TEXT("----------------->> 卖出股票K线更新完毕!!!"));
			//开始分析财务指标数据
			TArray<FString> allStocks;
			RecommendSellStocks_.GenerateKeyArray(allStocks);
			AnalyzeSellStockDatas(allStocks);
		}
	}

}
void URecommendStocksWidget::PlusGetF10Counts(){
	GetF10Counts++;
	UE_LOG(LogTemp, Warning, TEXT("------------>> 股票F10更新个数:%d"), GetF10Counts);
	if (GetF10Counts == RecommendStocks_.Num()) {//当股票F10刷新完开始执行市盈率分析
		UE_LOG(LogTemp, Warning, TEXT("----------------->> 股票F10更新完毕!!!"));
		if (GetKLineCounts == RecommendStocks_.Num()) {
			UE_LOG(LogTemp, Warning, TEXT("----------------->> 股票的K线数据也已经更新完毕,开始进行财务分析!!!"));
			//开始分析财务指标数据
			TArray<FString> allStocks;
			RecommendStocks_.GenerateKeyArray(allStocks);
			AnalyzeStockDatasForBuy(allStocks);
		}
	}
}

void URecommendStocksWidget::UpdateStockKLineF10(const TArray<FString> stockCodes,int buyOrSell){
	//更新这些股票的K线数据和F10财务数据
	GetKLineCounts = 0;
	GetF10Counts = 0;
	if (companyNameIndexWidgetBP == nullptr) { UE_LOG(LogTemp, Warning, TEXT("---------->>companyNameIndexWidgetBP == nullptr")); return; }
	if (!companyNameIndexWidgetBP->onFetchKLineDataToSave.IsBound()) {
		companyNameIndexWidgetBP->buyOrSell = buyOrSell;
		companyNameIndexWidgetBP->onFetchKLineDataToSave.AddDynamic(this, &URecommendStocksWidget::PlusGetKLineCounts);
	}
	float delaytimeForF10 = 1.0f, delaytimeForKLine = FMath::FRandRange(1.0f, 2.0f);
	for (const auto& stockCode : stockCodes) {
		if (NeedToDownLoadKLineFromInternet(stockCode)){
			FTimerHandle timerHandleForKLine;
			FTimerDelegate delegateForKLine;
			delegateForKLine.BindUObject(this, &URecommendStocksWidget::GetKLineDatasByStockCode, stockCode);
			GetWorld()->GetTimerManager().SetTimer(timerHandleForKLine, delegateForKLine, delaytimeForKLine, false);
			UE_LOG(LogTemp, Warning, TEXT("---------->>  %s 需要从网站获取K线数据, 将在%f秒后启动爬虫获取网站K线数据!"), *GetNameCode(stockCode), delaytimeForKLine);
			delaytimeForKLine += FMath::FRandRange(5.0f, 40.0f);
		}
		else { UE_LOG(LogTemp, Warning, TEXT("---------->>  %s K线数据本地有效,不需要从网站获取K线数据"),*GetNameCode(stockCode)); PlusGetKLineCounts(buyOrSell); }
		if (NeedToDownLoadF10FromInternet(stockCode)) {
			FTimerHandle timerHandleForF10;
			FTimerDelegate delegateForF10;
			delegateForF10.BindUObject(this, &URecommendStocksWidget::GetF10DatasByStockCode, stockCode);
			GetWorld()->GetTimerManager().SetTimer(timerHandleForF10, delegateForF10, delaytimeForF10, false);
			UE_LOG(LogTemp, Warning, TEXT("---------->>  %s 需要从网站获取F10数据, 将在%f秒后启动爬虫获取网站F10数据!"), *GetNameCode(stockCode), delaytimeForF10);
			delaytimeForF10 += FMath::FRandRange(2.0f, 10.0f);
		}
		else { UE_LOG(LogTemp, Warning, TEXT("---------->>  %s F10数据本地有效,不需要从网站获取F10数据"), *GetNameCode(stockCode)); PlusGetF10Counts(); }
	}
}

void URecommendStocksWidget::UpdateStockKLine(const TArray<FString> stockCodes, int buyOrSell){
	//更新这些股票的K线数据
	GetKLineCounts = 0;
	if (companyNameIndexWidgetBP == nullptr) { UE_LOG(LogTemp, Warning, TEXT("---------->>companyNameIndexWidgetBP == nullptr")); return; }
	if (!companyNameIndexWidgetBP->onFetchKLineDataToSave.IsBound()) {
		companyNameIndexWidgetBP->buyOrSell = buyOrSell;
		companyNameIndexWidgetBP->onFetchKLineDataToSave.AddDynamic(this, &URecommendStocksWidget::PlusGetKLineCounts);
	}
	float delaytimeForKLine = FMath::FRandRange(1.0f, 2.0f);
	for (const auto& stockCode : stockCodes) {
		if (NeedToDownLoadKLineFromInternet(stockCode)) {
			FTimerHandle timerHandleForKLine;
			FTimerDelegate delegateForKLine;
			delegateForKLine.BindUObject(this, &URecommendStocksWidget::GetKLineDatasByStockCode, stockCode);
			GetWorld()->GetTimerManager().SetTimer(timerHandleForKLine, delegateForKLine, delaytimeForKLine, false);
			UE_LOG(LogTemp, Warning, TEXT("---------->>  %s 需要从网站获取K线数据, 将在%f秒后启动爬虫获取网站K线数据!"), *GetNameCode(stockCode), delaytimeForKLine);
			delaytimeForKLine += FMath::FRandRange(6.0f, 60.0f);
		}
		else { UE_LOG(LogTemp, Warning, TEXT("---------->>  %s K线数据本地有效,不需要从网站获取K线数据"), *GetNameCode(stockCode)); PlusGetKLineCounts(buyOrSell); }
	}
}

void URecommendStocksWidget::AnalyzeStockDatasForBuy(const TArray<FString> stockCodes){
	progress = 0.0f;
	int tempI = 0;
	for (const auto& stockCode : stockCodes)	{
		tempI++;
		UE_LOG(LogTemp, Warning, TEXT("URecommendStocksWidget::AnalyzeStockDatas:: %s"), *stockCode);
		// 加载并验证F10数据
		FQTFinancialF10Main nestF10Data;
		if (!LoadLatestF10Datas(stockCode, nestF10Data)) {
			UE_LOG(LogTemp, Warning, TEXT("AnalyzeIndicatorsAndGetWeightAdjustment: %s 最新F10数据加载失败"), *GetNameCode(stockCode));
			continue;
		}
		// 加载技术指标数据
		FQTStockIndex nestKlineData;
		if (!LoadLatestTechnicalIndicators(stockCode, nestKlineData)) {
			UE_LOG(LogTemp, Warning, TEXT("AnalyzeIndicatorsAndGetWeightAdjustment: %s 最新技术指标加载失败"), *GetNameCode(stockCode));
			continue;
		}
		float Weight = AnalyzeStockForBuy(stockCode, nestKlineData, nestF10Data);

		// 最终权重
		RecommendStocks_[stockCode] += Weight;
		UE_LOG(LogTemp, Log, TEXT("%s 最终权重: %.2f"), *GetNameCode(stockCode), RecommendStocks_[stockCode]);
	}
	//分析完毕之后根据权重排序并选中前10只股票(如果前10只股票的推荐权重都>=0),推送给界面显示
	RecommendStocks_.ValueSort([](float A, float B) { return A > B; });
	UE_LOG(LogTemp, Warning, TEXT("分析完毕! 推荐股票排名:"));
	TArray<FString> stockNames, fisrt10stocks;
	RecommendStocks_.GenerateKeyArray(stockNames);
	for(int i=0;i<FMath::Min(10, stockNames.Num()); i++){
		FString stockCode = stockNames[i];
		float weight = RecommendStocks_[stockCode];
		if (weight >= 0.0f)	{
			fisrt10stocks.Add(stockCode);
			UE_LOG(LogTemp, Warning, TEXT("%d. %s (权重: %.2f)"), i + 1, *GetNameCode(stockCode), weight);
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("%d. %s (权重: %.2f) - 不推荐"), i + 1, *GetNameCode(stockCode), weight);
		}
	}
	//获取这些推荐股票的实时数据
	if (!stockMonitor_) stockMonitor_ = NewObject<UStockMonitor>(this);
	stockMonitor_->GetStocksDatas(fisrt10stocks);
}

void URecommendStocksWidget::AnalyzeSellStockDatas(const TArray<FString> stockCodes){
	//出手股票筛选步骤:8个技术指标(包括成交量Volume在内),有两个及以上的指标亮红灯,红灯个数越多越优先考虑;
	for (const auto& stockCode : stockCodes) {
		UE_LOG(LogTemp, Warning, TEXT("Analyzing stock for sell: %s"), *stockCode);
		//技术指标分析
		// 1. 加载技术指标数据
		FQTStockIndex Indicators;
		if (!LoadLatestTechnicalIndicators(stockCode, Indicators)) {
			UE_LOG(LogTemp, Warning, TEXT("AnalyzeIndicatorsForSell: %s 技术指标加载失败"), *GetNameCode(stockCode));
		}
		float Weight = AnalyzeIndicatorsForSell(stockCode, Indicators);
		// 最终权重
		RecommendSellStocks_[stockCode] += Weight;
		UE_LOG(LogTemp, Log, TEXT("%s 最终权重: %.2f"), *GetNameCode(stockCode), RecommendSellStocks_[stockCode]);
	}
	//分析完毕之后根据权重排序并选中前10只股票(如果前10只股票的推荐权重都>=0),推送给界面显示
	RecommendSellStocks_.ValueSort([](float A, float B) { return A > B; });
	UE_LOG(LogTemp, Warning, TEXT("分析完毕! 推荐出手股票排名:"));
	TArray<FString> stockNames, fisrt10stocks;
	RecommendSellStocks_.GenerateKeyArray(stockNames);
	for (int i = 0; i < FMath::Min(10, stockNames.Num()); i++) {
		FString stockCode = stockNames[i];
		float weight = RecommendSellStocks_[stockCode];
		if (weight >= 0.0f) {
			fisrt10stocks.Add(stockCode);
			UE_LOG(LogTemp, Warning, TEXT("%d. %s (权重: %.2f)"), i + 1, *GetNameCode(stockCode), weight);
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("%d. %s (权重: %.2f) - 不推荐"), i + 1, *GetNameCode(stockCode), weight);
		}
	}
	//获取这些推荐股票的实时数据
	if (!stockMonitor_) stockMonitor_ = NewObject<UStockMonitor>(this);
	stockMonitor_->GetStocksDatas(fisrt10stocks);
}

// 辅助函数：根据日期获取对应时期的EPS
float URecommendStocksWidget::GetEPSByDate(const TArray<TSharedPtr<FJsonValue>>* F10Datas, int32 KLineDate) {
	if (!F10Datas || F10Datas->Num() == 0)	{
		return 0.0f;
	}

	// 从最新到最旧查找，找到报告日期 <= K线日期的第一个数据
	for (int32 i = 0; i < F10Datas->Num(); ++i){
		const TSharedPtr<FJsonValue>& F10Value = (*F10Datas)[i];
		if (!F10Value.IsValid()) continue;

		const TSharedPtr<FJsonObject> F10Obj = F10Value->AsObject();
		if (!F10Obj || !F10Obj.IsValid()) continue;

		int32 ReportDate = 0;
		F10Obj->TryGetNumberField(TEXT("REPORTDATE"), ReportDate);

		// 如果报告日期 <= K线日期，使用这个EPS
		if (ReportDate <= KLineDate)	{
			float EPS = 0.0f;
			F10Obj->TryGetNumberField(TEXT("EPSJB"), EPS);
			return EPS;
		}
	}

	// 如果没找到，返回最新一期EPS
	const TSharedPtr<FJsonValue>& LatestF10 = (*F10Datas)[0];
	if (LatestF10.IsValid())	{
		const TSharedPtr<FJsonObject> LatestF10Obj = LatestF10->AsObject();
		if (LatestF10Obj && LatestF10Obj.IsValid()){
			float EPS = 0.0f;
			LatestF10Obj->TryGetNumberField(TEXT("EPSJB"), EPS);
			return EPS;
		}
	}
	return 0.0f;
}

FString URecommendStocksWidget::GetBusinessDescription(FString stockCode){
	FString fileContent;
	TSharedPtr<FQTStockListRow>  tempStockListRow = companyNameIndexWidgetBP->GetFQTStockListRowByCodeOrName(stockCode);//根据股票代码获取对应的本地文件路径
	FString introductionfilepath = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/KlineDatas/%s/Introduction.json"), *GetNameCode(stockCode));
	bool loadsuccesful = FFileHelper::LoadFileToString(fileContent, *introductionfilepath);
	if (!loadsuccesful) {
		UE_LOG(LogTemp, Warning, TEXT("GetBusinessDescription: %s公司简介加载失败"), *GetNameCode(stockCode)); 
		return "";
	}
	TSharedPtr<FJsonObject> jsonObject;
	TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(fileContent);
	if (!FJsonSerializer::Deserialize(jsonReader, jsonObject)) {
		UE_LOG(LogTemp, Warning, TEXT("GetBusinessDescription: %s公司简介内容序列化失败!"), *GetNameCode(stockCode));
		return "";
	}
	TSharedPtr<FJsonValue>introductionsValue = jsonObject->TryGetField(TEXT("CompanyIntroduction"));
	if (!introductionsValue.IsValid()) {
		UE_LOG(LogTemp, Warning, TEXT("GetBusinessDescription: %s公司简介内容为空"), *GetNameCode(stockCode));
		return "";
	}
	TSharedPtr<FJsonObject>introductionsObject = introductionsValue->AsObject();
	if (!introductionsObject.IsValid()) {
		UE_LOG(LogTemp, Warning, TEXT("GetBusinessDescription: %s公司简介转化JsonObject失败!"), *GetNameCode(stockCode));
		return "";
	}
	FString businessScope, mainBusiness;
	introductionsObject->TryGetStringField(TEXT("BUSINESS_SCOPE"), businessScope);
	introductionsObject->TryGetStringField(TEXT("MAIN_BUSINESS"), mainBusiness);
	return businessScope + mainBusiness;
}

float URecommendStocksWidget::AnalyzeBusinessAndGetWeightAdjustment(const FString& BusinessDescription, EBusinessCategory& OutCategory){
	OutCategory = EBusinessCategory::Unknown;
	if (BusinessDescription.IsEmpty()) {
		UE_LOG(LogTemp, Warning, TEXT("AnalyzeBusiness: 主营业务描述为空"));
		return 0.0f;
	}

	// 转换为小写便于匹配（中文不需要，但保留英文匹配）
	FString LowerDescription = BusinessDescription.ToLower();

	// 计算高科技匹配分数
	float HighTechScore = CalculateMatchScore(BusinessDescription, HighTechKeywords);
	// 计算销售代理匹配分数
	float SalesAgencyScore = CalculateMatchScore(BusinessDescription, SalesAgencyKeywords);

	UE_LOG(LogTemp, Log, TEXT("  高科技匹配度: %.2f, 销售代理匹配度: %.2f"), HighTechScore, SalesAgencyScore);

	// 判断业务类型
	if (HighTechScore > SalesAgencyScore && HighTechScore > 0.03f) {
		OutCategory = EBusinessCategory::HighTech;

		// 高科技板块：权重增加
		// 基础加成：0.5 ~ 3.0，根据匹配度调整
		float WeightBonus = FMath::Clamp(HighTechScore * 2.0f, 0.5f, 3.0f);

		// 特殊高精尖领域额外加成
		if (BusinessDescription.Contains(TEXT("芯片")) || BusinessDescription.Contains(TEXT("半导体")) || BusinessDescription.Contains(TEXT("人工智能")) || BusinessDescription.Contains(TEXT("云计算"))) {
			WeightBonus += 1.0f;
			UE_LOG(LogTemp, Log, TEXT("  核心高科技领域，额外+1.0"));
		}

		// 如果同时有部分销售代理特征，适当减少加成
		if (SalesAgencyScore > 0.2f) {
			WeightBonus *= 0.8f;
			UE_LOG(LogTemp, Log, TEXT("  检测到销售代理特征，加成降低20%%"));
		}

		UE_LOG(LogTemp, Log, TEXT("  判断为高科技板块，权重增加: +%.2f"), WeightBonus);
		return WeightBonus;
	}
	else if (SalesAgencyScore > 0.03f) {
		OutCategory = EBusinessCategory::SalesAgency;
		// 销售代理/经销：权重减少
		// 基础减分：-1.0 ~ -3.0，根据匹配度调整
		float WeightPenalty = -FMath::Clamp(SalesAgencyScore * 1.5f, 1.0f, 3.0f);
		// 纯贸易公司额外减分
		if (BusinessDescription.Contains(TEXT("贸易")) || BusinessDescription.Contains(TEXT("进出口")) || BusinessDescription.Contains(TEXT("批发"))) {
			WeightPenalty -= 0.8f;
			UE_LOG(LogTemp, Log, TEXT("  纯贸易型公司，额外-0.8"));
		}
		// 如果同时有高科技特征，减少减分幅度
		if (HighTechScore > 0.2f) {
			WeightPenalty *= 0.5f;
			UE_LOG(LogTemp, Log, TEXT("  检测到高科技特征，减分降低50%%"));
		}
		UE_LOG(LogTemp, Log, TEXT("  判断为销售代理/经销类型，权重减少: %.2f"), WeightPenalty);
		return WeightPenalty;
	}
	else {
		OutCategory = EBusinessCategory::Other;
		UE_LOG(LogTemp, Log, TEXT("  无法明确分类，权重不变"));
		return 0.0f;
	}
}

float URecommendStocksWidget::AnalyzeIndicatorsForSell(const FString& StockCode, const FQTStockIndex& Indicators){
	// 2. 判断每个指标的颜色
	TArray<EIndicatorColor> Colors;
	// Volume历史百分位
	EIndicatorColor VolumeColor = CheckVolumeIndicator(Indicators.HistoryVolumeRatio);
	Colors.Add(VolumeColor);
	// MACD
	EIndicatorColor MACDColor = CheckMACDIndicator(Indicators.MACD);
	Colors.Add(MACDColor);
	// KDJ
	EIndicatorColor KDJColor = CheckKDJIndicator(Indicators.KDJ_J);
	Colors.Add(KDJColor);
	// RSI
	EIndicatorColor RSIColor = CheckRSIIndicator(Indicators.RSI1, Indicators.RSI2);
	Colors.Add(RSIColor);
	// WR
	EIndicatorColor WRColor = CheckWRIndicator(Indicators.WR1, Indicators.WR2);
	Colors.Add(WRColor);
	// DMI
	EIndicatorColor DMIColor = CheckDMIIndicator(Indicators.PDI, Indicators.NDI, Indicators.ADX);
	Colors.Add(DMIColor);
	// CCI
	EIndicatorColor CCIColor = CheckCCIIndicator(Indicators.CCI);
	Colors.Add(CCIColor);
	// BIAS
	EIndicatorColor BIASColor = CheckBIASIndicator(Indicators.BIAS0, Indicators.BIAS1, Indicators.BIAS2);
	Colors.Add(BIASColor);
	StockIndicatorColors_.Add(StockCode, Colors);

	// 3. 统计红灯和绿灯个数
	int OutRedLightCount = 0, OutGreenLightCount = 0;
	for (EIndicatorColor Color : Colors) {
		if (Color == EIndicatorColor::Red) {
			OutRedLightCount++;
		}
		else if (Color == EIndicatorColor::Green) {
			OutGreenLightCount++;
		}
	}

	// 4. 日志输出
	UE_LOG(LogTemp, Log, TEXT("=== %s 技术指标分析 ==="), *StockCode);
	UE_LOG(LogTemp, Log, TEXT("Volume历史百分位: %.2f -> %s"), Indicators.HistoryVolumeRatio, VolumeColor == EIndicatorColor::Red ? TEXT("红灯") : (VolumeColor == EIndicatorColor::Green ? TEXT("绿灯") : TEXT("无灯")));
	UE_LOG(LogTemp, Log, TEXT("MACD: %.3f -> %s"), Indicators.MACD, MACDColor == EIndicatorColor::Red ? TEXT("红灯") : (MACDColor == EIndicatorColor::Green ? TEXT("绿灯") : TEXT("无灯")));
	UE_LOG(LogTemp, Log, TEXT("KDJ_J: %.2f -> %s"), Indicators.KDJ_J, KDJColor == EIndicatorColor::Red ? TEXT("红灯") : (KDJColor == EIndicatorColor::Green ? TEXT("绿灯") : TEXT("无灯")));
	UE_LOG(LogTemp, Log, TEXT("RSI: RSI2=%.2f, RSI3=%.2f -> %s"), Indicators.RSI1, Indicators.RSI2, RSIColor == EIndicatorColor::Red ? TEXT("红灯") : (RSIColor == EIndicatorColor::Green ? TEXT("绿灯") : TEXT("无灯")));
	UE_LOG(LogTemp, Log, TEXT("WR: WR1=%.2f, WR2=%.2f -> %s"), Indicators.WR1, Indicators.WR2, WRColor == EIndicatorColor::Red ? TEXT("红灯") : (WRColor == EIndicatorColor::Green ? TEXT("绿灯") : TEXT("无灯")));
	UE_LOG(LogTemp, Log, TEXT("DMI: PDI=%.2f, NDI=%.2f, ADX=%.2f -> %s"), Indicators.PDI, Indicators.NDI, Indicators.ADX, DMIColor == EIndicatorColor::Red ? TEXT("红灯") : (DMIColor == EIndicatorColor::Green ? TEXT("绿灯") : TEXT("无灯")));
	UE_LOG(LogTemp, Log, TEXT("CCI: %.2f -> %s"), Indicators.CCI, CCIColor == EIndicatorColor::Red ? TEXT("红灯") : (CCIColor == EIndicatorColor::Green ? TEXT("绿灯") : TEXT("无灯")));
	UE_LOG(LogTemp, Log, TEXT("BIAS: BIAS0=%.2f, BIAS1=%.2f, BIAS2=%.2f -> %s"), Indicators.BIAS0, Indicators.BIAS1, Indicators.BIAS2, BIASColor == EIndicatorColor::Red ? TEXT("红灯") : (BIASColor == EIndicatorColor::Green ? TEXT("绿灯") : TEXT("无灯")));
	UE_LOG(LogTemp, Log, TEXT("统计: 红灯=%d, 绿灯=%d"), OutRedLightCount, OutGreenLightCount);

	// 5. 计算权重调整
	float WeightAdjustment = CalculateWeightForSell(OutRedLightCount, OutGreenLightCount);
	UE_LOG(LogTemp, Log, TEXT("权重调整: %.2f"), WeightAdjustment);

	return WeightAdjustment;
}

float URecommendStocksWidget::AnalyzeStockForBuy(FString stockCode, const FQTStockIndex& Indicators, const FQTFinancialF10Main& inF10Data) {
	UE_LOG(LogTemp, Warning, TEXT("AnalyzeF10AndIndicatorsForBuy:: Analyzing stock: %s"), *GetNameCode(stockCode));

	// ==================== 财务指标筛选 ====================
	// ROE < 0 或 资产负债率 > 80 或 每股经营现金流 < 0，直接排除
	if (inF10Data.ROEJQ < 0 || inF10Data.ZCFZL > 80.0f || inF10Data.MGJYXJJE < 0) {
		UE_LOG(LogTemp, Warning, TEXT("%s 财务指标不合格: ROE=%.2f, 负债率=%.2f%%, 现金流=%.2f, 不推荐!"), *GetNameCode(stockCode), inF10Data.ROEJQ, inF10Data.ZCFZL, inF10Data.MGJYXJJE);
		return -100.0f;
	}

	// 计算基础权重
	float Weight = 0.0f;
	// 资产负债率 < 60% 优先
	if (inF10Data.ZCFZL < 60.0f) {
		float DebtBonus = (60.0f - inF10Data.ZCFZL) * 0.1f;
		Weight += DebtBonus;
		UE_LOG(LogTemp, Log, TEXT("%s 资产负债率%.2f%%, 加分%.2f"), *GetNameCode(stockCode), inF10Data.ZCFZL, DebtBonus);
	}
	// 每股经营现金流 > 每股收益 优先
	if (inF10Data.MGJYXJJE > inF10Data.EPSJB) {
		float CashBonus = FMath::Min((inF10Data.MGJYXJJE - inF10Data.EPSJB) * 4.0f, 20.0f);
		Weight += CashBonus;
		UE_LOG(LogTemp, Log, TEXT("%s 现金流%.2f > EPS%.2f, 加分%.2f"), *GetNameCode(stockCode), inF10Data.MGJYXJJE, inF10Data.EPSJB, CashBonus);
	}
	// ROE 越高越优先
	float ROEBonus = FMath::Min(inF10Data.ROEJQ * 0.5f, 20.0f);
	Weight += ROEBonus;
	UE_LOG(LogTemp, Log, TEXT("%s ROE=%.2f%%, 加分%.2f"), *GetNameCode(stockCode), inF10Data.ROEJQ, ROEBonus);

	// 根据历史百分位调整权重
	if (Indicators.HistoryPEPercentile > 0.7f) {
		UE_LOG(LogTemp, Warning, TEXT("%s 市盈率历史百分位%.2f%%, 不推荐!"), *GetNameCode(stockCode), Indicators.HistoryPEPercentile);
		return -100.0f;
	}
	else if (Indicators.HistoryPEPercentile < 0.3f) {
		float PercentileBonus = FMath::Square(1.0f - Indicators.HistoryPEPercentile) * 10.0f;
		Weight += PercentileBonus;
		UE_LOG(LogTemp, Log, TEXT("%s 市盈率历史百分位低, 加分%.2f"), *GetNameCode(stockCode), PercentileBonus);
	}

	//主营业务分析(高科技板块的股票推荐权重增加,销售代理类型的公司推荐权重减小)
	FString businessDescription = GetBusinessDescription(stockCode);
	EBusinessCategory businessCategory;
	Weight += 4.0f * URecommendStocksWidget::AnalyzeBusinessAndGetWeightAdjustment(businessDescription, businessCategory);
	//技术指标分析
	// 2. 判断每个指标的颜色
	TArray<EIndicatorColor> Colors;
	// Volume历史百分位
	EIndicatorColor VolumeColor = URecommendStocksWidget::CheckVolumeIndicator(Indicators.HistoryVolumeRatio);
	Colors.Add(VolumeColor);
	// MACD
	EIndicatorColor MACDColor = URecommendStocksWidget::CheckMACDIndicator(Indicators.MACD);
	Colors.Add(MACDColor);
	// KDJ
	EIndicatorColor KDJColor = URecommendStocksWidget::CheckKDJIndicator(Indicators.KDJ_J);
	Colors.Add(KDJColor);
	// RSI
	EIndicatorColor RSIColor = URecommendStocksWidget::CheckRSIIndicator(Indicators.RSI1, Indicators.RSI2);
	Colors.Add(RSIColor);
	// WR
	EIndicatorColor WRColor = URecommendStocksWidget::CheckWRIndicator(Indicators.WR1, Indicators.WR2);
	Colors.Add(WRColor);
	// DMI
	EIndicatorColor DMIColor = URecommendStocksWidget::CheckDMIIndicator(Indicators.PDI, Indicators.NDI, Indicators.ADX);
	Colors.Add(DMIColor);
	// CCI
	EIndicatorColor CCIColor = URecommendStocksWidget::CheckCCIIndicator(Indicators.CCI);
	Colors.Add(CCIColor);
	// BIAS
	EIndicatorColor BIASColor = URecommendStocksWidget::CheckBIASIndicator(Indicators.BIAS0, Indicators.BIAS1, Indicators.BIAS2);
	Colors.Add(BIASColor);
	StockIndicatorColors_.Add(stockCode, Colors);
	// 3. 统计红灯和绿灯个数
	int OutRedLightCount = 0, OutGreenLightCount = 0;
	for (EIndicatorColor Color : Colors) {
		if (Color == EIndicatorColor::Red) {
			OutRedLightCount++;
		}
		else if (Color == EIndicatorColor::Green) {
			OutGreenLightCount++;
		}
	}

	// 4. 日志输出
	UE_LOG(LogTemp, Log, TEXT("=== %s 技术指标分析 ==="), *GetNameCode(stockCode));
	UE_LOG(LogTemp, Log, TEXT("Volume历史百分位: %.2f -> %s"), Indicators.HistoryVolumeRatio, VolumeColor == EIndicatorColor::Red ? TEXT("红灯") : (VolumeColor == EIndicatorColor::Green ? TEXT("绿灯") : TEXT("无灯")));
	UE_LOG(LogTemp, Log, TEXT("MACD: %.3f -> %s"), Indicators.MACD, MACDColor == EIndicatorColor::Red ? TEXT("红灯") : (MACDColor == EIndicatorColor::Green ? TEXT("绿灯") : TEXT("无灯")));
	UE_LOG(LogTemp, Log, TEXT("KDJ_J: %.2f -> %s"), Indicators.KDJ_J, KDJColor == EIndicatorColor::Red ? TEXT("红灯") : (KDJColor == EIndicatorColor::Green ? TEXT("绿灯") : TEXT("无灯")));
	UE_LOG(LogTemp, Log, TEXT("RSI: RSI2=%.2f, RSI3=%.2f -> %s"), Indicators.RSI1, Indicators.RSI2, RSIColor == EIndicatorColor::Red ? TEXT("红灯") : (RSIColor == EIndicatorColor::Green ? TEXT("绿灯") : TEXT("无灯")));
	UE_LOG(LogTemp, Log, TEXT("WR: WR1=%.2f, WR2=%.2f -> %s"), Indicators.WR1, Indicators.WR2, WRColor == EIndicatorColor::Red ? TEXT("红灯") : (WRColor == EIndicatorColor::Green ? TEXT("绿灯") : TEXT("无灯")));
	UE_LOG(LogTemp, Log, TEXT("DMI: PDI=%.2f, NDI=%.2f, ADX=%.2f -> %s"), Indicators.PDI, Indicators.NDI, Indicators.ADX, DMIColor == EIndicatorColor::Red ? TEXT("红灯") : (DMIColor == EIndicatorColor::Green ? TEXT("绿灯") : TEXT("无灯")));
	UE_LOG(LogTemp, Log, TEXT("CCI: %.2f -> %s"), Indicators.CCI, CCIColor == EIndicatorColor::Red ? TEXT("红灯") : (CCIColor == EIndicatorColor::Green ? TEXT("绿灯") : TEXT("无灯")));
	UE_LOG(LogTemp, Log, TEXT("BIAS: BIAS0=%.2f, BIAS1=%.2f, BIAS2=%.2f -> %s"), Indicators.BIAS0, Indicators.BIAS1, Indicators.BIAS2, BIASColor == EIndicatorColor::Red ? TEXT("红灯") : (BIASColor == EIndicatorColor::Green ? TEXT("绿灯") : TEXT("无灯")));
	UE_LOG(LogTemp, Log, TEXT("统计: 红灯=%d, 绿灯=%d"), OutRedLightCount, OutGreenLightCount);

	// 5. 计算权重调整
	float WeightAdjustment = URecommendStocksWidget::CalculateWeight(OutRedLightCount, OutGreenLightCount);
	UE_LOG(LogTemp, Log, TEXT("交易指标红绿灯的权重调整: %.2f"), WeightAdjustment);
	Weight += WeightAdjustment;
	UE_LOG(LogTemp, Log, TEXT("最终推荐权重: %.2f"), Weight);

	return Weight;
}

bool URecommendStocksWidget::LoadLatestTechnicalIndicators(const FString& StockCode, FQTStockIndex& klineIndicators){
	// 构建K线文件路径
	FString KLineFilePath = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/KlineDatas/%s/Kline101.json"), *GetNameCode(StockCode));
	if (!FPaths::FileExists(KLineFilePath)){
		UE_LOG(LogTemp, Warning, TEXT("Kline101文件不存在: %s"), *KLineFilePath);
		return false;
	}
	// 加载文件
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *KLineFilePath)) {
		UE_LOG(LogTemp, Warning, TEXT("KLine文件加载失败: %s"), *KLineFilePath);
		return false;
	}

	// 解析JSON
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid()) {
		UE_LOG(LogTemp, Warning, TEXT("KLine JSON解析失败: %s"), *GetNameCode(StockCode));
		return false;
	}
	// 获取K线数组
	const TArray<TSharedPtr<FJsonValue>>* KLineArray = nullptr;
	if (!JsonObject->TryGetArrayField(TEXT("klines"), KLineArray) || !KLineArray || KLineArray->Num() == 0) {
		UE_LOG(LogTemp, Warning, TEXT("KLine数据为空: %s"), *GetNameCode(StockCode));
		return false;
	}
	// 获取最新一条K线数据（最后一条）
	const TSharedPtr<FJsonValue>& LatestKLine = KLineArray->Last();
	if (!LatestKLine.IsValid()) {
		UE_LOG(LogTemp, Warning, TEXT("最新K线数据无效: %s"), *GetNameCode(StockCode));
		return false;
	}
	const TSharedPtr<FJsonObject>& LatestKLineObj = LatestKLine->AsObject();
	if (!LatestKLineObj || !LatestKLineObj.IsValid()) {
		UE_LOG(LogTemp, Warning, TEXT("最新K线不是JSON对象: %s"), *GetNameCode(StockCode));
		return false;
	}
	// 读取各项技术指标
	LatestKLineObj->TryGetNumberField(TEXT("HistoryVolumeRatio"), klineIndicators.HistoryVolumeRatio);
	LatestKLineObj->TryGetNumberField(TEXT("HistoryPEPercentile"), klineIndicators.HistoryPEPercentile);
	LatestKLineObj->TryGetNumberField(TEXT("MACD"), klineIndicators.MACD);
	LatestKLineObj->TryGetNumberField(TEXT("KDJ_K"), klineIndicators.KDJ_K);
	LatestKLineObj->TryGetNumberField(TEXT("KDJ_D"), klineIndicators.KDJ_D);
	LatestKLineObj->TryGetNumberField(TEXT("KDJ_J"), klineIndicators.KDJ_J);
	LatestKLineObj->TryGetNumberField(TEXT("RSI0"), klineIndicators.RSI0);
	LatestKLineObj->TryGetNumberField(TEXT("RSI1"), klineIndicators.RSI1);
	LatestKLineObj->TryGetNumberField(TEXT("RSI2"), klineIndicators.RSI2);
	LatestKLineObj->TryGetNumberField(TEXT("WR1"), klineIndicators.WR1);
	LatestKLineObj->TryGetNumberField(TEXT("WR2"), klineIndicators.WR2);
	LatestKLineObj->TryGetNumberField(TEXT("PDI"), klineIndicators.PDI);
	LatestKLineObj->TryGetNumberField(TEXT("NDI"), klineIndicators.NDI);
	LatestKLineObj->TryGetNumberField(TEXT("ADX"), klineIndicators.ADX);
	LatestKLineObj->TryGetNumberField(TEXT("CCI"), klineIndicators.CCI);
	LatestKLineObj->TryGetNumberField(TEXT("BIAS0"), klineIndicators.BIAS0);
	LatestKLineObj->TryGetNumberField(TEXT("BIAS1"), klineIndicators.BIAS1);
	LatestKLineObj->TryGetNumberField(TEXT("BIAS2"), klineIndicators.BIAS2);
	return true;
}

bool URecommendStocksWidget::LoadLatestF10Datas(const FString& stockCode, FQTFinancialF10Main& f10Datas){
	// 构建K线文件路径
	FString f10FilePath = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/KlineDatas/%s/F10.json"), *GetNameCode(stockCode));
	if (!FPaths::FileExists(f10FilePath)) {
		UE_LOG(LogTemp, Warning, TEXT("URecommendStocksWidget::LoadLatestF10Datas::F10文件不存在: %s"), *f10FilePath);
		return false;
	}
	// 加载文件
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *f10FilePath)) {
		UE_LOG(LogTemp, Warning, TEXT("URecommendStocksWidget::LoadLatestF10Datas::F10文件加载失败: %s"), *f10FilePath);
		return false;
	}
	// 解析JSON
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid()) {
		UE_LOG(LogTemp, Warning, TEXT("URecommendStocksWidget::LoadLatestF10Datas::F10 JSON解析失败: %s"), *GetNameCode(stockCode));
		return false;
	}
	// 获取F10数组
	const TArray<TSharedPtr<FJsonValue>>* f10DatasArray = nullptr;
	if (!JsonObject->TryGetArrayField(TEXT("F10"), f10DatasArray) || !f10DatasArray || f10DatasArray->Num() == 0) {
		UE_LOG(LogTemp, Warning, TEXT("URecommendStocksWidget::LoadLatestF10Datas::F10数据为空: %s"), *GetNameCode(stockCode));
		return false;
	}
	// 获取最新一条F10线数据（第一条）
	const TSharedPtr<FJsonValue>& LatestF10 = (*f10DatasArray)[0];
	if (!LatestF10.IsValid()) {
		UE_LOG(LogTemp, Warning, TEXT("URecommendStocksWidget::LoadLatestF10Datas::最新F10数据无效: %s"), *GetNameCode(stockCode));
		return false;
	}
	const TSharedPtr<FJsonObject>& LatestF10Obj = LatestF10->AsObject();
	if (!LatestF10Obj || !LatestF10Obj.IsValid()) {
		UE_LOG(LogTemp, Warning, TEXT("URecommendStocksWidget::LoadLatestF10Datas::最新F10不是JSON对象: %s"), *GetNameCode(stockCode));
		return false;
	}
	// 读取各项数据
	LatestF10Obj->TryGetStringField(TEXT("SECURITY_CODE"), f10Datas.SECURITY_CODE);
	LatestF10Obj->TryGetStringField(TEXT("SECURITY_NAME_ABBR"), f10Datas.SECURITY_NAME_ABBR);
	LatestF10Obj->TryGetNumberField(TEXT("REPORTDATE"), f10Datas.REPORTDATE);
	LatestF10Obj->TryGetNumberField(TEXT("EPSJB"), f10Datas.EPSJB);
	LatestF10Obj->TryGetNumberField(TEXT("EPSXS"), f10Datas.EPSXS);
	LatestF10Obj->TryGetNumberField(TEXT("BPS"), f10Datas.BPS);
	LatestF10Obj->TryGetNumberField(TEXT("MGJYXJJE"), f10Datas.MGJYXJJE);
	LatestF10Obj->TryGetNumberField(TEXT("MLR"), f10Datas.MLR);
	LatestF10Obj->TryGetNumberField(TEXT("PARENTNETPROFIT"), f10Datas.PARENTNETPROFIT);
	LatestF10Obj->TryGetNumberField(TEXT("ROEJQ"), f10Datas.ROEJQ);
	LatestF10Obj->TryGetNumberField(TEXT("ZZCJLL"), f10Datas.ZZCJLL);
	LatestF10Obj->TryGetNumberField(TEXT("XSMLL"), f10Datas.XSMLL);
	LatestF10Obj->TryGetNumberField(TEXT("XSJLL"), f10Datas.XSJLL);
	LatestF10Obj->TryGetNumberField(TEXT("ZCFZL"), f10Datas.ZCFZL);
	return true;
}

EIndicatorColor URecommendStocksWidget::CheckVolumeIndicator(float HistoryVolumeRatio){
	if (HistoryVolumeRatio > 0.7f) return EIndicatorColor::Red;
	if (HistoryVolumeRatio < 0.3f) return EIndicatorColor::Green;
	return EIndicatorColor::None;
}

EIndicatorColor URecommendStocksWidget::CheckMACDIndicator(float MACD){
	if (MACD > 0.2f)	return EIndicatorColor::Red;
	if (MACD < -0.2f)	 return EIndicatorColor::Green;
	return EIndicatorColor::None;
}

EIndicatorColor URecommendStocksWidget::CheckKDJIndicator(float KDJ_J){
	if (KDJ_J > 90.0f)		return EIndicatorColor::Red;
	if (KDJ_J < 10.0f)		return EIndicatorColor::Green;
	return EIndicatorColor::None;
}

EIndicatorColor URecommendStocksWidget::CheckRSIIndicator(float RSI2, float RSI3){
	// RSI2 > RSI3 && RSI2 > 50 闪红灯
	if (RSI2 > RSI3 && RSI2 > 50.0f)		return EIndicatorColor::Red;
	// RSI2 < RSI3 && RSI2 < 50 闪绿灯
	if (RSI2 < RSI3 && RSI2 < 50.0f)		return EIndicatorColor::Green;
	return EIndicatorColor::None;
}

EIndicatorColor URecommendStocksWidget::CheckWRIndicator(float WR1, float WR2){
	// WR1和WR2同时大于90闪红灯
	if (WR1 > 90.0f && WR2 > 90.0f)		return EIndicatorColor::Red;
	// WR1和WR2同时小于10闪绿灯
	if (WR1 < 10.0f && WR2 < 10.0f)		return EIndicatorColor::Green;
	return EIndicatorColor::None;
}

EIndicatorColor URecommendStocksWidget::CheckDMIIndicator(float PDI, float NDI, float ADX){
	// PDI > NDI && ADX > 20 闪红灯
	if (PDI > NDI && ADX > 20.0f)		return EIndicatorColor::Red;
	// NDI > PDI && ADX > 20 闪绿灯
	if (NDI > PDI && ADX > 20.0f)		return EIndicatorColor::Green;
	return EIndicatorColor::None;
}

EIndicatorColor URecommendStocksWidget::CheckCCIIndicator(float CCI){
	if (CCI > 100.0f)		return EIndicatorColor::Red;
	if (CCI < 0.0f)		return EIndicatorColor::Green;
	return EIndicatorColor::None;
}

EIndicatorColor URecommendStocksWidget::CheckBIASIndicator(float BIAS0, float BIAS1, float BIAS2){
	// BIAS0 > 3 && BIAS1 > 5 && BIAS2 > 10 闪红灯
	if (BIAS0 > 3.0f && BIAS1 > 5.0f && BIAS2 > 10.0f)		return EIndicatorColor::Red;
	// BIAS0 < -3 && BIAS1 < -5 && BIAS2 < -10 闪绿灯
	if (BIAS0 < -3.0f && BIAS1 < -5.0f && BIAS2 < -10.0f)		return EIndicatorColor::Green;
	return EIndicatorColor::None;
}

float URecommendStocksWidget::CalculateWeight(int32 RedLightCount, int32 GreenLightCount){
	// 红灯个数 > 绿灯个数的直接筛掉
	if (RedLightCount > GreenLightCount) {
		UE_LOG(LogTemp, Log, TEXT("红灯个数%d > 绿灯个数%d，直接排除"), RedLightCount, GreenLightCount);
		return -100.0f;  // 大幅降低权重，相当于排除
	}

	// 绿灯个数越多，推荐权重越大
	// 基础权重：每个绿灯 +2 分
	float BaseWeight = GreenLightCount * 2.0f;
	// 红灯惩罚：每个红灯 -1 分
	float Penalty = RedLightCount * 1.0f;
	// 净权重
	float Weight = BaseWeight - Penalty;

	// 额外奖励：如果绿灯 >= 6 个，额外 +5 分
	if (GreenLightCount >= 6) {
		Weight += 5.0f;
		UE_LOG(LogTemp, Log, TEXT("绿灯 >= 6个，额外推荐 权重+5"));
	}
	// 额外惩罚：如果红灯 >= 3 个，额外 -3 分
	if (RedLightCount >= 3) {
		Weight -= 3.0f;
		UE_LOG(LogTemp, Log, TEXT("红灯 >= 3个，额外推荐 权重 -3"));
	}

	// 限制范围：-10 到 +20
	Weight = FMath::Clamp(Weight, -10.0f, 20.0f);
	UE_LOG(LogTemp, Log, TEXT("权重计算: 绿灯+%d分, 红灯-%d分, 最终=%.2f"), GreenLightCount * 2, RedLightCount, Weight);
	return Weight;
}

float URecommendStocksWidget::CalculateWeightForSell(int32 RedLightCount, int32 GreenLightCount){
	// 红灯个数 < 2 的或红灯个数少于绿灯个数的直接筛掉
	if (RedLightCount < GreenLightCount || RedLightCount < 2) {
		UE_LOG(LogTemp, Log, TEXT("CalculateWeightForSell::红灯个数%d < 2偏少或绿灯个数%d偏多，直接排除"), RedLightCount, GreenLightCount);
		return -100.0f;  // 大幅降低权重，相当于排除
	}
	// 红灯个数越多，推荐权重越大
	// 基础权重：每个红灯 +2 分
	float BaseWeight = RedLightCount * 2.0f;
	// 绿灯惩罚：每个绿灯 -1 分
	float Penalty = GreenLightCount * 1.0f;
	// 净权重
	float Weight = BaseWeight - Penalty;

	// 额外奖励：如果红灯 >= 6 个，额外 +5 分
	if (RedLightCount >= 6) {
		Weight += 5.0f;
		UE_LOG(LogTemp, Log, TEXT("CalculateWeightForSell::红灯 >= 6个，额外推荐 权重+5"));
	}
	// 额外惩罚：如果绿灯 >= 3 个，额外 -3 分
	if (GreenLightCount >= 3) {
		Weight -= 3.0f;
		UE_LOG(LogTemp, Log, TEXT("CalculateWeightForSell::绿灯 >= 3个，额外推荐 权重 -3"));
	}

	// 限制范围：-10 到 +20
	Weight = FMath::Clamp(Weight, -10.0f, 20.0f);
	UE_LOG(LogTemp, Log, TEXT("CalculateWeightForSell::权重计算: 红灯+%d分, 绿灯-%d分, 最终=%.2f"), RedLightCount * 2, GreenLightCount, Weight);
	return Weight;
}

const TArray<FString> URecommendStocksWidget::HighTechKeywords = {
	// 信息技术类
	TEXT("软件"), TEXT("软件开发"), TEXT("云计算"), TEXT("大数据"), TEXT("人工智能"),
	TEXT("AI"), TEXT("物联网"), TEXT("IoT"), TEXT("区块链"), TEXT("5G"),
	TEXT("芯片"), TEXT("半导体"), TEXT("集成电路"), TEXT("电子设计"), TEXT("EDA"),
	// 互联网类
	TEXT("互联网"), TEXT("移动互联网"), TEXT("电商平台"), TEXT("网络游戏"), TEXT("手游"),
	TEXT("社交平台"), TEXT("在线教育"), TEXT("在线医疗"), TEXT("SaaS"), TEXT("云服务"),
	// 硬件类
	TEXT("智能硬件"), TEXT("消费电子"), TEXT("通信设备"), TEXT("光通信"), TEXT("传感器"),
	TEXT("机器人"), TEXT("无人机"), TEXT("自动驾驶"), TEXT("汽车电子"), TEXT("安防监控"),
	// 新兴技术类
	TEXT("生物科技"), TEXT("基因测序"), TEXT("创新药"), TEXT("医疗器械"), TEXT("高端装备"),
	TEXT("新材料"), TEXT("新能源"), TEXT("光伏"), TEXT("锂电池"), TEXT("储能"),
	TEXT("航天"), TEXT("军工"), TEXT("量子计算"), TEXT("虚拟现实"), TEXT("AR/VR"),
	// 研发类
	TEXT("研发"), TEXT("技术开发"), TEXT("专利"), TEXT("核心技术"), TEXT("自主可控"),
	TEXT("国产替代"), TEXT("专精特新"), TEXT("高新技术"), TEXT("科技园")
};

const TArray<FString> URecommendStocksWidget::SalesAgencyKeywords = {
	// 销售代理类
	TEXT("代理"), TEXT("经销"), TEXT("分销"), TEXT("贸易"), TEXT("进出口"),
	TEXT("批发"), TEXT("零售"), TEXT("连锁"), TEXT("门店"), TEXT("专卖店"),
	TEXT("加盟"), TEXT("特许经营"), TEXT("渠道"), TEXT("销售网络"), TEXT("营销"),
	// 商贸类
	TEXT("商贸"), TEXT("物资流通"), TEXT("供应链管理"), TEXT("物流"), TEXT("仓储"),
	TEXT("配送"), TEXT("电商代运营"), TEXT("品牌代理"), TEXT("独家代理"),
	// 零售类
	TEXT("百货"), TEXT("超市"), TEXT("便利店"), TEXT("商场"), TEXT("购物中心"),
	TEXT("免税店"), TEXT("奥特莱斯"), TEXT("折扣店"), TEXT("社区店"),
	// 中介服务
	TEXT("中介"), TEXT("经纪"), TEXT("咨询"), TEXT("外包"), TEXT("服务代理")
};

float URecommendStocksWidget::CalculateMatchScore(const FString& Text, const TArray<FString>& Keywords){
	if (Text.IsEmpty() || Keywords.Num() == 0) return 0.0f;

	float TotalScore = 0.0f;
	int32 MatchCount = 0;
	float MaxPossibleScore = 0.0f;
	// 计算实际匹配得分和理论最大得分
	for (const FString& Keyword : Keywords) {
		// 关键词权重（更长更具体的关键词权重更高）
		float KeywordWeight = FMath::Clamp(Keyword.Len() / 10.0f, 0.5f, 2.0f);
		MaxPossibleScore += KeywordWeight;  // 累加理论最大得分

		if (Text.Contains(Keyword)) {
			TotalScore += KeywordWeight;
			MatchCount++;
		}
	}

	if (MatchCount == 0) return 0.0f;

	// 方法1：基于实际得分与最大可能得分的比例（考虑关键词权重）
	float NormalizedScore = TotalScore / MaxPossibleScore;
	// 结果范围: 0.0 ~ 1.0
	UE_LOG(LogTemp, Verbose, TEXT("匹配: %d/%d个关键词, 得分: %.2f/%.2f, 归一化: %.2f"), MatchCount, Keywords.Num(), TotalScore, MaxPossibleScore, NormalizedScore);
	return NormalizedScore;
}
