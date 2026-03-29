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
			tempObject->SetStringField(TEXT("REPORT_DATE"), tempF10Data.REPORT_DATE );
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
	UE_LOG(LogTemp, Warning, TEXT("---------->>  GetKLineDatasByStockCode::正在从网站获取%s的K线数据 JustSave"), *stockCode);
	companyNameIndexWidgetBP->FetchKLineDataJustSave(stockCode);
}

bool URecommendStocksWidget::NeedToDownLoadKLineFromInternet(FString stockCode) {
	FString fileContent;
	TSharedPtr<FQTStockListRow>  tempStockListRow = companyNameIndexWidgetBP->GetFQTStockListRowByCodeOrName(stockCode);//根据股票代码获取对应的本地文件路径
	FString klinefilepath = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/KlineDatas/%s/Kline101.json"), *(tempStockListRow->NAMECODE));
	bool loadsuccesful = FFileHelper::LoadFileToString(fileContent, *klinefilepath);
	if (!loadsuccesful) {//如果文件不存在,就需要从网站获取日线数据并保存到本地文件
		return true;
	}
	TSharedPtr<FJsonObject> jsonObject;
	TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(fileContent);
	if (FJsonSerializer::Deserialize(jsonReader, jsonObject) && jsonObject.IsValid()) {
		int fetchedTime;
		jsonObject->TryGetNumberField(TEXT("FetchedAt"), fetchedTime);
		int currentTime = FDateTime::Now().GetYear() * 10000 + FDateTime::Now().GetMonth() * 100 + FDateTime::Now().GetDay();
		EDayOfWeek weekday = FDateTime::Now().GetDayOfWeek();
		if (weekday == EDayOfWeek::Saturday)currentTime -= 1;//如果是周六就往前推1天
		if (weekday == EDayOfWeek::Sunday)currentTime -= 2;//如果是周日就往前推2天
		if (fetchedTime < currentTime) {//如果数据超过一天没更新,就重新从网站获取日线数据并保存到本地文件
			return true;
		}
	}
	return false;
}

void URecommendStocksWidget::GetF10DatasByStockCode(FString stockCode){
	if (!stockMonitor_) stockMonitor_ = NewObject<UStockMonitor>();
	UE_LOG(LogTemp, Warning, TEXT("---------->>  GetKLineDatasByStockCode::正在从网站获取%s的F10数据 JustSave"), *stockCode);
	stockMonitor_->GetStockF10FianceMainDatas(stockCode);
}

bool URecommendStocksWidget::NeedToDownLoadF10FromInternet(FString stockCode){
	FString fileContent;
	TSharedPtr<FQTStockListRow>  tempStockListRow = companyNameIndexWidgetBP->GetFQTStockListRowByCodeOrName(stockCode);//根据股票代码获取对应的本地文件路径
	FString f10filepath = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/KlineDatas/%s/F10.json"), *(tempStockListRow->NAMECODE));
	bool loadsuccesful = FFileHelper::LoadFileToString(fileContent, *f10filepath);
	if (!loadsuccesful) {//如果文件不存在,就需要从网站获取日线数据并保存到本地文件
		return true;
	}
	TSharedPtr<FJsonObject> jsonObject;
	TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(fileContent);
	if (FJsonSerializer::Deserialize(jsonReader, jsonObject) && jsonObject.IsValid()) {
		int fetchedTime;
		jsonObject->TryGetNumberField(TEXT("FetchedAt"), fetchedTime);
		int currentTime = FDateTime::Now().GetYear() * 10000 + FDateTime::Now().GetMonth() * 100 + FDateTime::Now().GetDay();
		EDayOfWeek weekday = FDateTime::Now().GetDayOfWeek();
		if (weekday == EDayOfWeek::Saturday)currentTime -= 1;//如果是周六就往前推1天
		if (weekday == EDayOfWeek::Sunday)currentTime -= 2;//如果是周日就往前推2天
		if (fetchedTime < currentTime) {//如果数据超过一天没更新,就需要重新从网站获取日线数据并保存到本地文件
			return true;
		}
	}
	return false;
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
	for (const auto& stockCode : stockCodes) {
		FString fileContent;
		TSharedPtr<FQTStockListRow>  tempStockListRow = companyNameIndexWidgetBP->GetFQTStockListRowByCodeOrName(stockCode);//根据股票代码获取对应的本地文件路径
		FString f10filepath = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/KlineDatas/%s/F10.json"), *(tempStockListRow->NAMECODE));
		bool loadsuccesful = FFileHelper::LoadFileToString(fileContent, *f10filepath);
		if (!loadsuccesful) {
			UE_LOG(LogTemp, Warning, TEXT("---------------------------->> %s本地F10财务数据加载失败!"), *f10filepath);
			continue;
		}
		TSharedPtr<FJsonObject> jsonObject;
		TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(fileContent);
		if (FJsonSerializer::Deserialize(jsonReader, jsonObject) && jsonObject.IsValid()) {
			const TArray<TSharedPtr<FJsonValue>>* f10datas;
			if (jsonObject->TryGetArrayField(TEXT("F10"), f10datas)) {
				TSharedPtr<FJsonObject> data0Object = (*f10datas)[0]->AsObject();//目前只取最新的一条财务数据进行分析,后续可以考虑增加对历史财务数据的分析
				FString f10Data0_SECURITY_CODE;
				float f10Data0_ROEJQ, f10Data0_ZCFZL, f10Data0_MGJYXJJE, f10Data0_EPSJB;
				bool getnumberSuccessful = true;
				getnumberSuccessful=data0Object->TryGetStringField(TEXT("SECURITY_CODE"), f10Data0_SECURITY_CODE);
				getnumberSuccessful=data0Object->TryGetNumberField(TEXT("ROEJQ"), f10Data0_ROEJQ);
				getnumberSuccessful=data0Object->TryGetNumberField(TEXT("ZCFZL"), f10Data0_ZCFZL);
				getnumberSuccessful=data0Object->TryGetNumberField(TEXT("MGJYXJJE"), f10Data0_MGJYXJJE);
				getnumberSuccessful=data0Object->TryGetNumberField(TEXT("EPSJB"), f10Data0_EPSJB);
				if (!getnumberSuccessful) { UE_LOG(LogTemp, Warning, TEXT("---------------------------->> %s 分析F10财务数据有指标数据缺失!!"), *(tempStockListRow->NAMECODE)); return; }
				//开始解析财务数据,ROE<0的一律筛掉,ROE越大越优先推荐,其次再检查资产负债率,大于80%的筛掉,小于60%的优先.最后再检查现金流,小于0的筛掉,每股现金流>每股净收益的优先考虑;
				if (f10Data0_ROEJQ < 0 || f10Data0_ZCFZL>80 || f10Data0_MGJYXJJE < 0) {
					UE_LOG(LogTemp, Warning, TEXT("---------------------------->> %s 净资产收益率 < 0 || 资产负债率>80 || 每股经营现金流 < 0, 不推荐!"), *(tempStockListRow->NAMECODE));
					RecommendStocks_[f10Data0_SECURITY_CODE] -= 100;//权重值大幅降低,相当于直接筛掉了这个股票
					return;
				}
				if (f10Data0_ZCFZL < 60 || f10Data0_MGJYXJJE > f10Data0_EPSJB) {
					UE_LOG(LogTemp, Warning, TEXT("---------------------------->> %s 资产负债率<60 || 每股经营现金流 > 基本每股净收益, 优先推荐!"), *(tempStockListRow->NAMECODE));
					RecommendStocks_[f10Data0_SECURITY_CODE] += 2 * (0.6f - f10Data0_ZCFZL);//资产负债率小于60%的优先考虑,权重值增加2倍的差值
					RecommendStocks_[f10Data0_SECURITY_CODE] += 0.5f * (f10Data0_MGJYXJJE - f10Data0_EPSJB);//每股经营现金流大于每股净收益的优先考虑,权重值增加0.5倍的差值
					RecommendStocks_[f10Data0_SECURITY_CODE] += f10Data0_ROEJQ * 3;//ROE越大越优先推荐,权重值增加ROE*3
				}
				//开始分析市盈率

			}
			else { UE_LOG(LogTemp, Warning, TEXT("---------------------------->> %s 分析F10财务数据缺失F10字段"), *f10filepath); }
		}
		else { UE_LOG(LogTemp, Warning, TEXT("---------------------------->> %s 分析F10财务数据序列化失败"), *f10filepath); }
	}
}
