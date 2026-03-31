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

void URecommendStocksWidget::StartFilterStocks(){
	bool success = false;
	success = LoadStocksFromRecentStockListJson({ TEXT("*ST"),TEXT("ST") });//首先筛选掉所有*ST和ST开头的股票,因为这些股票通常是有退市风险的,不适合推荐给用户
	if (success) {//然后对RecommendStocks_里面的股票逐一进行财务基本面分析,ROE<0的一律筛掉,ROE越大越优先推荐,其次再检查资产负债率,大于80%的筛掉,小于60%的优先.最后再检查现金流,小于0的筛掉,每股现金流>每股净收益的优先考虑;
		TArray<FString> allStocks;
		RecommendStocks_.GenerateKeyArray(allStocks);
		UpdateStockKLineF10(allStocks);//首先触发更新K线和财务的函数,那边会相隔不固定的时间段逐一更新RecommendStocks_里面的所有股票K线数据和F10数据;更新完之后会触发财指标分析函数
	}
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
	TSharedPtr<FQTStockListRow>  tempStockListRow = companyNameIndexWidgetBP->GetFQTStockListRowByCodeOrName(stockCode);//根据股票代码获取对应的本地文件路径
	FString klinefilepath = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/KlineDatas/%s/Kline101.json"), *GetNameCode(stockCode));
	bool loadsuccesful = FFileHelper::LoadFileToString(fileContent, *klinefilepath);
	if (!loadsuccesful) {//如果加载失败返回false
		return false;
	}
	TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(fileContent);
	return FJsonSerializer::Deserialize(jsonReader, outJsonObj);
}

void URecommendStocksWidget::GetF10DatasByStockCode(FString stockCode){
	if (!stockMonitor_) stockMonitor_ = NewObject<UStockMonitor>();
	UE_LOG(LogTemp, Warning, TEXT("---------->>  GetKLineDatasByStockCode::正在从网站获取%s的F10数据 JustSave"), *GetNameCode(stockCode));
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
	TSharedPtr<FQTStockListRow>  tempStockListRow = companyNameIndexWidgetBP->GetFQTStockListRowByCodeOrName(stockCode);//根据股票代码获取对应的本地文件路径
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

void URecommendStocksWidget::PlusGetKLineCounts(){
	GetKLineCounts++;
	UE_LOG(LogTemp, Warning, TEXT("------------>> 股票K线更新个数:%d"), GetKLineCounts);
	if (GetKLineCounts == RecommendStocks_.Num()) {//当股票K线刷新完开始执行市盈率分析
		UE_LOG(LogTemp, Warning, TEXT("----------------->> 股票K线更新完毕!!!"));
		if (GetF10Counts == RecommendStocks_.Num()) {
			UE_LOG(LogTemp, Warning, TEXT("----------------->> 股票的F10财务数据也已经更新完毕,开始进行财务分析!!!"));
			//开始分析财务指标数据
			TArray<FString> allStocks;
			RecommendStocks_.GenerateKeyArray(allStocks);
			AnalyzeF10Datas(allStocks);
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
			AnalyzeF10Datas(allStocks);
		}
	}
}

void URecommendStocksWidget::UpdateStockKLineF10(const TArray<FString> stockCodes){
	//更新这些股票的K线数据和F10财务数据
	GetKLineCounts = 0;
	GetF10Counts = 0;
	if (companyNameIndexWidgetBP == nullptr) { UE_LOG(LogTemp, Warning, TEXT("---------->>companyNameIndexWidgetBP == nullptr")); return; }
	if (!companyNameIndexWidgetBP->onFetchKLineDataToSave.IsBound())	companyNameIndexWidgetBP->onFetchKLineDataToSave.AddDynamic(this, &URecommendStocksWidget::PlusGetKLineCounts);
	int iforKLine = 0;//计数,记录需要从网站获取K线数据的股票数目
	int iforF10 = 0;//计数,记录需要从网站获取F10数据的股票数目
	for (const auto& stockCode : stockCodes) {
		if (NeedToDownLoadKLineFromInternet(stockCode)){
			++iforKLine;
			FTimerHandle timerHandleForKLine;
			FTimerDelegate delegateForKLine;
			delegateForKLine.BindUObject(this, &URecommendStocksWidget::GetKLineDatasByStockCode, stockCode);
			GetWorld()->GetTimerManager().SetTimer(timerHandleForKLine, delegateForKLine, iforKLine + FMath::FRandRange(1.0f, 5.0f), false);
		}
		else { UE_LOG(LogTemp, Warning, TEXT("---------->>  %s K线数据本地有效,不需要从网站获取K线数据"),*stockCode); PlusGetKLineCounts(); }
		if (NeedToDownLoadF10FromInternet(stockCode)) {
			++iforF10;
			FTimerHandle timerHandleForF10;
			FTimerDelegate delegateForF10;
			delegateForF10.BindUObject(this, &URecommendStocksWidget::GetF10DatasByStockCode, stockCode);
			GetWorld()->GetTimerManager().SetTimer(timerHandleForF10, delegateForF10, iforF10 + FMath::FRandRange(1.0f, 4.0f), false);
		}
		else { UE_LOG(LogTemp, Warning, TEXT("---------->>  %s F10数据本地有效,不需要从网站获取F10数据"), *stockCode); PlusGetF10Counts(); }
	}
}

void URecommendStocksWidget::AnalyzeF10Datas(const TArray<FString> stockCodes){
	for (const auto& stockCode : stockCodes)	{
		UE_LOG(LogTemp, Warning, TEXT("Analyzing stock: %s"), *stockCode);
		// ==================== 1. 加载并验证F10数据 ====================
		TSharedPtr<FJsonObject> F10JsonObject;
		bool bLoadSuccess = LoadLocalF10Data(stockCode, F10JsonObject);
		if (!bLoadSuccess || !F10JsonObject.IsValid())	{
			UE_LOG(LogTemp, Warning, TEXT("%s 本地F10财务数据加载失败!"), *GetNameCode(stockCode));
			continue;
		}
		// 获取F10数组
		const TArray<TSharedPtr<FJsonValue>>* F10Datas = nullptr;
		if (!F10JsonObject->TryGetArrayField(TEXT("F10"), F10Datas) || !F10Datas) {
			UE_LOG(LogTemp, Warning, TEXT("%s 缺少F10字段"), *GetNameCode(stockCode));
			continue;
		}
		if (F10Datas->Num() == 0)	{
			UE_LOG(LogTemp, Warning, TEXT("%s F10数据为空"), *GetNameCode(stockCode));
			continue;
		}
		// ==================== 2. 解析最新财务数据 ====================
		const TSharedPtr<FJsonValue>& LatestF10Value = (*F10Datas)[0];
		if (!LatestF10Value.IsValid()){
			UE_LOG(LogTemp, Warning, TEXT("%s 最新F10数据无效"), *GetNameCode(stockCode));
			continue;
		}
		const TSharedPtr<FJsonObject> LatestF10Object = LatestF10Value->AsObject();
		if (!LatestF10Object || !LatestF10Object.IsValid()){
			UE_LOG(LogTemp, Warning, TEXT("%s F10数据不是JSON对象"), *GetNameCode(stockCode));
			continue;
		}

		// 安全获取各字段（分别检查每个字段）
		FString SecurityCode;
		float ROE = 0.0f, DebtRatio = 0.0f, CashFlowPerShare = 0.0f, EPS = 0.0f;
		bool bHasAllFields = true;
		bHasAllFields &= LatestF10Object->TryGetStringField(TEXT("SECURITY_CODE"), SecurityCode);
		bHasAllFields &= LatestF10Object->TryGetNumberField(TEXT("ROEJQ"), ROE);
		bHasAllFields &= LatestF10Object->TryGetNumberField(TEXT("ZCFZL"), DebtRatio);
		bHasAllFields &= LatestF10Object->TryGetNumberField(TEXT("MGJYXJJE"), CashFlowPerShare);
		bHasAllFields &= LatestF10Object->TryGetNumberField(TEXT("EPSJB"), EPS);
		if (!bHasAllFields)	{
			UE_LOG(LogTemp, Warning, TEXT("%s F10财务数据缺失必要字段"), *GetNameCode(stockCode));
			continue;
		}

		// ==================== 3. 财务指标筛选 ====================
		// ROE < 0 或 资产负债率 > 80 或 每股经营现金流 < 0，直接排除
		if (ROE < 0 || DebtRatio > 80.0f || CashFlowPerShare < 0)	{
			UE_LOG(LogTemp, Warning, TEXT("%s 财务指标不合格: ROE=%.2f, 负债率=%.2f%%, 现金流=%.2f, 不推荐!"), *GetNameCode(stockCode), ROE, DebtRatio, CashFlowPerShare);
			RecommendStocks_[SecurityCode] -= 100.0f;
			continue;
		}

		// 计算基础权重
		float Weight = 0.0f;
		// 资产负债率 < 60% 优先
		if (DebtRatio < 60.0f){
			float DebtBonus = (60.0f - DebtRatio) * 0.2f;
			Weight += DebtBonus;
			UE_LOG(LogTemp, Log, TEXT("%s 资产负债率%.2f%%, 加分%.2f"),	*GetNameCode(stockCode), DebtRatio, DebtBonus);
		}

		// 每股经营现金流 > 每股收益 优先
		if (CashFlowPerShare > EPS){
			float CashBonus = (CashFlowPerShare - EPS) * 0.5f;
			Weight += CashBonus;
			UE_LOG(LogTemp, Log, TEXT("%s 现金流%.2f > EPS%.2f, 加分%.2f"), *GetNameCode(stockCode), CashFlowPerShare, EPS, CashBonus);
		}

		// ROE 越高越优先
		float ROEBonus = ROE * 3.0f;
		Weight += ROEBonus;
		UE_LOG(LogTemp, Log, TEXT("%s ROE=%.2f%%, 加分%.2f"), *GetNameCode(stockCode), ROE, ROEBonus);

		// ==================== 4. 加载K线数据 ====================
		TSharedPtr<FJsonObject> KLineJsonObject;
		bLoadSuccess = LoadLocalKLineData(stockCode, KLineJsonObject);
		if (!bLoadSuccess || !KLineJsonObject.IsValid()){
			UE_LOG(LogTemp, Warning, TEXT("%s K线数据加载失败"), *GetNameCode(stockCode));
			RecommendStocks_[SecurityCode] += Weight;
			continue;
		}
		const TArray<TSharedPtr<FJsonValue>>* KLineDatas = nullptr;
		if (!KLineJsonObject->TryGetArrayField(TEXT("Klines"), KLineDatas) || !KLineDatas || KLineDatas->Num() == 0){
			UE_LOG(LogTemp, Warning, TEXT("%s K线数据缺失或为空"), *GetNameCode(stockCode));
			RecommendStocks_[SecurityCode] += Weight;
			continue;
		}

		// ==================== 5. 计算历史市盈率百分位 ====================
		// 先获取最新收盘价
		const TSharedPtr<FJsonValue>& LatestKLine = KLineDatas->Last();
		if (!LatestKLine.IsValid()){
			UE_LOG(LogTemp, Warning, TEXT("%s 最新K线数据无效"), *GetNameCode(stockCode));
			RecommendStocks_[SecurityCode] += Weight;
			continue;
		}
		const TSharedPtr<FJsonObject> LatestKLineObj = LatestKLine->AsObject();
		if (!LatestKLineObj || !LatestKLineObj.IsValid()){
			UE_LOG(LogTemp, Warning, TEXT("%s 最新K线不是JSON对象"), *GetNameCode(stockCode));
			RecommendStocks_[SecurityCode] += Weight;
			continue;
		}

		float LatestClosePrice = 0.0f;
		LatestKLineObj->TryGetNumberField(TEXT("Close"), LatestClosePrice);

		// 计算最新市盈率（使用最新EPS）
		float LatestPE = (EPS > 0.001f) ? (LatestClosePrice / EPS) : 0.0f;
		if (LatestPE <= 0.0f)	{
			UE_LOG(LogTemp, Warning, TEXT("%s 市盈率计算无效: 股价=%.2f, EPS=%.2f"), *GetNameCode(stockCode), LatestClosePrice, EPS);
			RecommendStocks_[SecurityCode] += Weight;
			continue;
		}

		// 遍历历史K线，计算历史市盈率百分位
		int32 SmallerCount = 0;
		int32 ValidCount = 0;

		for (int32 i = 0; i < KLineDatas->Num(); ++i)	{
			const TSharedPtr<FJsonValue>& KLineValue = (*KLineDatas)[i];
			if (!KLineValue.IsValid()) continue;
			const TSharedPtr<FJsonObject> KLineObj = KLineValue->AsObject();
			if (!KLineObj || !KLineObj.IsValid()) continue;
			// 获取该日期的收盘价
			float ClosePrice = 0.0f;
			KLineObj->TryGetNumberField(TEXT("Close"), ClosePrice);
			// 获取对应时期的EPS（根据日期匹配）
			int32 KLineDate = 0;
			KLineObj->TryGetNumberField(TEXT("Date"), KLineDate);

			float PeriodEPS = GetEPSByDate(F10Datas, KLineDate);
			if (PeriodEPS <= 0.0f) continue;

			float PeriodPE = ClosePrice / PeriodEPS;
			if (PeriodPE <= 0.0f) continue;

			ValidCount++;
			if (PeriodPE < LatestPE){
				SmallerCount++;
			}
		}
		// 计算历史百分位
		float HistoryPEPercentile = (ValidCount > 0) ? (float)SmallerCount / ValidCount * 100.0f : 50.0f;

		UE_LOG(LogTemp, Log, TEXT("%s 当前PE=%.2f, 历史百分位=%.2f%% (%d/%d)"), *GetNameCode(stockCode), LatestPE, HistoryPEPercentile, SmallerCount, ValidCount);

		// 根据历史百分位调整权重
		if (HistoryPEPercentile > 70.0f){
			UE_LOG(LogTemp, Warning, TEXT("%s 市盈率历史百分位%.2f%%, 不推荐!"), *GetNameCode(stockCode), HistoryPEPercentile);
			RecommendStocks_[SecurityCode] -= 100.0f;
			continue;
		}
		else if (HistoryPEPercentile < 30.0f){
			float PercentileBonus = (100.0f - HistoryPEPercentile) * 0.2f;
			Weight += PercentileBonus;
			UE_LOG(LogTemp, Log, TEXT("%s 市盈率历史百分位低, 加分%.2f"), *GetNameCode(stockCode), PercentileBonus);
		}

		//主营业务分析(高科技板块的股票推荐权重增加,销售代理类型的公司推荐权重减小)
		FString businessDescription = GetBusinessDescription(stockCode);
		EBusinessCategory businessCategory;
		Weight += AnalyzeBusinessAndGetWeightAdjustment(businessDescription, businessCategory);
		//技术指标分析


		// 最终权重
		RecommendStocks_[SecurityCode] += Weight;
		UE_LOG(LogTemp, Log, TEXT("%s 最终权重: %.2f"), *GetNameCode(stockCode), RecommendStocks_[SecurityCode]);
	}
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

	UE_LOG(LogTemp, Log, TEXT("业务分析: %s"), *BusinessDescription);
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
