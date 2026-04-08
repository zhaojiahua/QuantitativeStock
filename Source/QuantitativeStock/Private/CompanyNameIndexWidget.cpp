
#include "CompanyNameIndexWidget.h"
#include "QuantitativeTradingCanves.h"
#include "StockListDownWidget.h"
#include "RegexMatchedWidget.h"
#include "StockMonitor.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

void UCompanyNameIndexWidget::GetIntroductionByCodeOrName(FString codeOrName){
	TSharedPtr<FQTStockListRow>* sourcecodeptr = StockRowListMap_.Find(codeOrName);
	if (sourcecodeptr) {
		TSharedPtr<FQTStockListRow> sourcecode = *sourcecodeptr;
		//文件格式: 公司简称+股票代码,例如: 贵州茅台600519(如果公司简称有*号开头,则替换为^号)
		FString companynamecode = sourcecode->NAMECODE.Replace(TEXT("*"), TEXT("^"));
		currentFilename_ = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/KlineDatas/%s/Kline101.json"), *companynamecode);
		FString IntroductionPath = currentFilename_.Replace(TEXT("Kline101"), TEXT("Introduction"));
		FString fileContent;
		bool loadsuccesful = FFileHelper::LoadFileToString(fileContent, *IntroductionPath);
		if (loadsuccesful) {
			TSharedPtr<FJsonObject> jsonObject;
			TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(fileContent);
			if (FJsonSerializer::Deserialize(jsonReader, jsonObject) && jsonObject.IsValid()) {
				int fetchedTime;
				jsonObject->TryGetNumberField(TEXT("FetchedAt"), fetchedTime);
				int currentTime = FDateTime::Now().GetYear() * 10000 + FDateTime::Now().GetMonth() * 100 + FDateTime::Now().GetDay();
				if (fetchedTime < currentTime) {//如果数据超过一天没更新,就重新从网站获取公司介绍数据
					UE_LOG(LogTemp, Warning, TEXT("---------->> 公司介绍数据文件超过一天未更新,正在重新从网站获取数据"));
					FetchCompanyIntroductionData(codeOrName);
					return;
				}
				TSharedPtr<FJsonValue> jsonValue = jsonObject->TryGetField(TEXT("CompanyIntroduction"));
				if (!jsonValue.IsValid()) return;
				TSharedPtr<FJsonObject> jsonObj = jsonValue->AsObject();
				TSharedPtr<FQTCompanyAbstractRow> currentCompanyIntroduction_ = MakeShareable(new FQTCompanyAbstractRow());
				currentCompanyIntroduction_->CompanyName = jsonObj->GetStringField(TEXT("ORG_NAME"));
				currentCompanyIntroduction_->EnglishName = jsonObj->GetStringField(TEXT("ORG_NAME_EN"));
				currentCompanyIntroduction_->OldSimplifyName = jsonObj->GetStringField(TEXT("FORMERNAME"));
				currentCompanyIntroduction_->IndexCodeA = jsonObj->GetStringField(TEXT("STR_CODEA"));
				currentCompanyIntroduction_->SimplifyNameA = jsonObj->GetStringField(TEXT("STR_NAMEA"));
				currentCompanyIntroduction_->IndexCodeB = jsonObj->GetStringField(TEXT("STR_CODEB"));
				currentCompanyIntroduction_->SimplifyNameB = jsonObj->GetStringField(TEXT("STR_NAMEB"));
				currentCompanyIntroduction_->IndexCodeH = jsonObj->GetStringField(TEXT("STR_CODEH"));
				currentCompanyIntroduction_->SimplifyNameH = jsonObj->GetStringField(TEXT("STR_NAMEH"));
				currentCompanyIntroduction_->MarketSegment = jsonObj->GetStringField(TEXT("TRADE_MARKET"));
				currentCompanyIntroduction_->Industry = jsonObj->GetStringField(TEXT("INDUSTRYCSRC1"));
				currentCompanyIntroduction_->LegalRepresentative = jsonObj->GetStringField(TEXT("LEGAL_PERSON"));
				currentCompanyIntroduction_->RegisteredCapital = jsonObj->GetStringField(TEXT("REG_CAPITAL"));
				currentCompanyIntroduction_->EstablishmentDate = jsonObj->GetStringField(TEXT("FOUND_DATE"));
				currentCompanyIntroduction_->ListingDate = jsonObj->GetStringField(TEXT("LISTING_DATE"));
				currentCompanyIntroduction_->OfficialWebsite = jsonObj->GetStringField(TEXT("ORG_WEB"));
				currentCompanyIntroduction_->Email = jsonObj->GetStringField(TEXT("ORG_EMAIL"));
				currentCompanyIntroduction_->Telephone = jsonObj->GetStringField(TEXT("ORG_TEL"));
				currentCompanyIntroduction_->RegisteredAddress = jsonObj->GetStringField(TEXT("REG_ADDRESS"));
				currentCompanyIntroduction_->OfficeAddress = jsonObj->GetStringField(TEXT("ADDRESS"));
				currentCompanyIntroduction_->PostalCode = jsonObj->GetStringField(TEXT("ADDRESS_POSTCODE"));
				currentCompanyIntroduction_->MainBusiness = jsonObj->GetStringField(TEXT("MAIN_BUSINESS"));
				currentCompanyIntroduction_->ScopeOfBusiness = jsonObj->GetStringField(TEXT("BUSINESS_SCOPE"));
				currentCompanyIntroduction_->OrganizationIntroduction = jsonObj->GetStringField(TEXT("ORG_PROFILE"));
				onIntroductionUpdate.Broadcast(*currentCompanyIntroduction_);
				return;
			}
			else UE_LOG(LogTemp, Warning, TEXT("---------->> 公司介绍数据文件JSON反序列化失败"));
		}
		else {//文件不存在,从网站获取公司介绍数据
			UE_LOG(LogTemp, Warning, TEXT("---------->> 公司介绍数据文件不存在: %s"), *IntroductionPath);
			FetchCompanyIntroductionData(codeOrName);
		}
	}
	else UE_LOG(LogTemp, Warning, TEXT("----------------------------------------------------->> 股票不存在!"));
}

void UCompanyNameIndexWidget::GetKLineDatasBP(const FString& codeOrName, int inklt, int  infqt){
	TSharedPtr<FQTStockListRow>* sourcecodeptr = StockRowListMap_.Find(codeOrName);
	if (sourcecodeptr) {
		if (GetKLineDatasByStockCode((*sourcecodeptr)->CODE, inklt, infqt) && outKLineDatas_.Num() > 0 && mainCanvas)	mainCanvas->OnCompanyCommitted(outKLineDatas_, true);
	}
	else UE_LOG(LogTemp,Warning,TEXT("----------------------------------------------------->> 股票不存在!"));
}

void UCompanyNameIndexWidget::SaveDownListStocksBP(const FString& codeOrName, const FString& filename){
	GetRecentStockList(filename);
	TSharedPtr<FQTStockListRow>* sourcecodeptr = StockRowListMap_.Find(codeOrName);
	if (sourcecodeptr) {
		if (DownStockList_.Contains(*sourcecodeptr)) DownStockList_.Remove(*sourcecodeptr);
		DownStockList_.AddUnique(*sourcecodeptr);
		SaveRecentStockList(filename);
	}

}

void UCompanyNameIndexWidget::SaveToRecentListPathBP(const FString& codeOrName){
	FString fileContent;
	FString recentFilename = FPaths::ProjectDir() + FString("Saved/StockDatas/RecentStockList.json");
	if (FFileHelper::LoadFileToString(fileContent, *recentFilename)) {
		TSharedPtr<FJsonObject> rootObj;
		TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(fileContent);
		if (FJsonSerializer::Deserialize(jsonReader, rootObj) && rootObj.IsValid()) {
			TArray< TSharedPtr <FJsonValue>> recentStocks = rootObj->GetArrayField(TEXT("StockList"));
			if (recentStocks.IsEmpty()) { UE_LOG(LogTemp, Error, TEXT("---------->> RecentStockList的StockList为空!")); }
			for (int i = 0; i < recentStocks.Num(); ++i) {
				FString stockcode, stockname;
				recentStocks[i]->AsObject()->TryGetStringField(TEXT("CODE"), stockcode);
				recentStocks[i]->AsObject()->TryGetStringField(TEXT("NAME"), stockname);
				if (stockcode == codeOrName || stockname == codeOrName) {
					recentStocks.RemoveAt(i);
					break;
				}
			}
			TSharedPtr<FJsonObject> newStockObj = MakeShareable(new FJsonObject());
			TSharedPtr<FQTStockListRow>* sourcecodeptr = StockRowListMap_.Find(codeOrName);
			if (sourcecodeptr) {
				newStockObj->SetStringField(TEXT("CODE"), (*sourcecodeptr)->CODE);
				newStockObj->SetStringField(TEXT("NAME"), (*sourcecodeptr)->NAME);
				newStockObj->SetStringField(TEXT("CODEMARK"), (*sourcecodeptr)->CODEMARK);
				newStockObj->SetStringField(TEXT("NAMECODE"), (*sourcecodeptr)->NAMECODE);
			}
			recentStocks.Add(MakeShareable(new FJsonValueObject(newStockObj)));
			rootObj->SetArrayField(TEXT("StockList"), recentStocks);
			FString outputString;
			TSharedRef<TJsonWriter<>> jsonWriter = TJsonWriterFactory<>::Create(&outputString);
			if (FJsonSerializer::Serialize(rootObj.ToSharedRef(), jsonWriter)) {
				FFileHelper::SaveStringToFile(outputString, *recentFilename);
				UE_LOG(LogTemp, Log, TEXT("---------->> 成功保存到最近访问列表!"));
			}
		}
		else UE_LOG(LogTemp, Error, TEXT("---------->> RecentStockList历史数据反序列化失败!"));
	}
	else UE_LOG(LogTemp, Error, TEXT("---------->> 读取RecentStockList历史失败!"));
}

void UCompanyNameIndexWidget::SavePreStockDownListDatasFromDownListWidget(const FString& savedFile){
	if (savedFile == "RecentStockList.json" || savedFile == "Null")return;
	TArray<FString> currentDownListDatas_JustCode;
	if (stockListDownWidgetBP && stockListDownWidgetBP->GetCurrentDownListDatas(currentDownListDatas_JustCode)) {
		DownStockList_.Empty();
		for (const auto& item : currentDownListDatas_JustCode) {
			TSharedPtr<FQTStockListRow>* sourcecodeptr = StockRowListMap_.Find(item);
			if (sourcecodeptr) {
				DownStockList_.AddUnique(*sourcecodeptr);
			}
		}
		SaveRecentStockList(savedFile);
	}
}

void UCompanyNameIndexWidget::GetStockDownListDatas(const FString& savedFile){
	GetRecentStockList(savedFile);
	if (stockMonitor_ && DownStockList_.Num()>0) {
		TArray<FString> codes;
		for (const auto& item : DownStockList_) {
			codes.Add(item->CODE);
		}
		//stockMonitor_->GetStocksDatas(codes);
		{//测试用,实际应该从网站获取最新数据,避免频繁请求网站
			listStocksDatas_.Empty();
			for (auto& item : DownStockList_) {
				FQTStockRealTimeData stockData;
				stockData.StockCode = item->CODE;
				stockData.StockName = item->NAME;
				stockData.LatestPrice = FMath::FRandRange(5.0, 10.0);
				listStocksDatas_.Add(stockData);
			}
			stockListDownWidgetBP->UpdateStockListDatas(listStocksDatas_);
		}
	}
}

void UCompanyNameIndexWidget::RegexListDatas(const FString& nameorcode){
	TArray<TSharedPtr<FQTStockListRow>> matchedStocks;
	//在股票列表中进行正则匹配
	for (const auto& item : StockRowListMap_) {
		if (item.Key.Contains(nameorcode)) {
			matchedStocks.Add(item.Value);
		}
	}
	//在基金列表中进行正则匹配
	for (const auto& item : FundRowListMap_) {
		if (item.Key.Contains(nameorcode)) {
			matchedStocks.Add(item.Value);
		}
	}
	if (matchedStocks.Num() > 100) {
		TArray<TSharedPtr<FQTStockListRow>> first10matchedStocks;
		for(int i=0;i<100;++i){
			first10matchedStocks.Add(matchedStocks[i]);
		}
		//更新下拉列表显示
		RegexMatchedWidgetBP->UpdateMatchedDatas(first10matchedStocks);
	}
	else RegexMatchedWidgetBP->UpdateMatchedDatas(matchedStocks);
}

void UCompanyNameIndexWidget::HandleStockDataResponse(const FString& ResponseData, int32 insource) {
	UE_LOG(LogTemp, Warning, TEXT("---------------------------->> 接收到数据开始解析"));
	FQTStockRealTimeData stockData;
	bool bParseSuccess = false;
	if (insource == 0) bParseSuccess = stockMonitor_->ParseEMResponse(ResponseData, stockData);
	else if (insource == 1) bParseSuccess = stockMonitor_->ParseTencentResponse(ResponseData, listStocksDatas_);
	else if (insource == 2) bParseSuccess = stockMonitor_->ParseSinaResponse(ResponseData, stockData);
	if (bParseSuccess) {
		//解析成功,触发股票列表数据更新
		stockListDownWidgetBP->UpdateStockListDatas(listStocksDatas_);
		// 打印日志
		UE_LOG(LogTemp, Log, TEXT("---------------------------->> 股票列表数据更新: %d条"), listStocksDatas_.Num());
	}
	else 	UE_LOG(LogTemp, Warning, TEXT("---------------------------->> 解析股票数据失败: %s"), *ResponseData);
}
void UCompanyNameIndexWidget::HandleF10DataResponse(const FString& ResponseData, int32 insource) { return; }
void UCompanyNameIndexWidget::HandleStockDataError(int32 ErrorCode, const FString& ErrorMessage) {
	UE_LOG(LogTemp, Error, TEXT("股票数据请求错误: %d - %s"), ErrorCode, *ErrorMessage);
}

bool UCompanyNameIndexWidget::GetKLineDatasByStockCode(const FString& stockCode, int inklt, int  infqt) {
		FString fileContent;
		bool loadsuccesful = FFileHelper::LoadFileToString(fileContent, *currentFilename_);
		if (!loadsuccesful) {//如果文件不存在,就从网站获取日线数据并保存到本地文件
				UE_LOG(LogTemp, Warning, TEXT("---------->> 股票日线数据文件不存在,正在从网站获取数据"));
				FetchKLineData(stockCode, inklt, infqt);
				return false;
		}
		UE_LOG(LogTemp, Warning, TEXT("---------->> 正在加载股票日线数据文件: %s"), *currentFilename_);
		TSharedPtr<FJsonObject> jsonObject;
		TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(fileContent);
		if (FJsonSerializer::Deserialize(jsonReader, jsonObject) && jsonObject.IsValid()) {
			int fetchedTime;
			jsonObject->TryGetNumberField(TEXT("FetchedAt"), fetchedTime);
			int currentTime = FDateTime::Now().GetYear() * 10000 + FDateTime::Now().GetMonth() * 100 + FDateTime::Now().GetDay();
			EDayOfWeek weekday = FDateTime::Now().GetDayOfWeek();
			if (weekday == EDayOfWeek::Saturday)currentTime -= 1;//如果是周六就往前推1天
			if (weekday == EDayOfWeek::Sunday)currentTime -= 2;//如果是周日就往前推2天
			if(fetchedTime < currentTime) {//如果数据超过一天没更新,就重新从网站获取日线数据并保存到本地文件
				UE_LOG(LogTemp, Warning, TEXT("---------->> 股票日线数据文件超过一天未更新,正在重新从网站获取数据"));
				FetchKLineData(stockCode, inklt, infqt);
				return false;
			}
			else {//本地数据有效,直接更新K线数据组
				UE_LOG(LogTemp, Warning, TEXT("---------->> %s日线数据本地数据有效直接加载本地数据"), *currentFilename_);
				const TArray<TSharedPtr<FJsonValue>>* klineArray;
				if (jsonObject->TryGetArrayField(TEXT("Klines"), klineArray)) {
					outKLineDatas_.Empty();//清空之前的数据
					for (auto& eachKLineValue : *klineArray) {
						TSharedPtr<FJsonObject> klineObject = eachKLineValue->AsObject();
						if (klineObject.IsValid()) {
							TSharedPtr<FQTStockIndex> klineData = MakeShareable(new FQTStockIndex());
							klineData->Date = klineObject->TryGetField(TEXT("Date"))->AsNumber();
							klineData->Open = klineObject->TryGetField(TEXT("Open"))->AsNumber();
							klineData->Close = klineObject->TryGetField(TEXT("Close"))->AsNumber();
							klineData->High = klineObject->TryGetField(TEXT("High"))->AsNumber();
							klineData->Low = klineObject->TryGetField(TEXT("Low"))->AsNumber();
							klineData->Change = klineObject->TryGetField(TEXT("Change"))->AsNumber();
							klineData->ChangeRatio = klineObject->TryGetField(TEXT("ChangeRatio"))->AsNumber();
							klineData->Volume = klineObject->TryGetField(TEXT("Volume"))->AsNumber();
							klineData->Turnover = klineObject->TryGetField(TEXT("Turnover"))->AsNumber();
							klineData->PriceRange = klineObject->TryGetField(TEXT("PriceRange"))->AsNumber();
							klineData->TurnoverRate = klineObject->TryGetField(TEXT("TurnoverRate"))->AsNumber();
							klineData->ConsNumber = klineObject->TryGetField(TEXT("ConsNumber"))->AsNumber();
							klineData->SMA5 = klineObject->TryGetField(TEXT("SMA5"))->AsNumber();
							klineData->SMA5SUM = klineObject->TryGetField(TEXT("SMA5SUM"))->AsNumber();
							klineData->SMA10 = klineObject->TryGetField(TEXT("SMA10"))->AsNumber();
							klineData->SMA10SUM = klineObject->TryGetField(TEXT("SMA10SUM"))->AsNumber();
							klineData->SMA20 = klineObject->TryGetField(TEXT("SMA20"))->AsNumber();
							klineData->SMA20SUM = klineObject->TryGetField(TEXT("SMA20SUM"))->AsNumber();
							klineData->SMA60 = klineObject->TryGetField(TEXT("SMA60"))->AsNumber();
							klineData->SMA60SUM = klineObject->TryGetField(TEXT("SMA60SUM"))->AsNumber();
							klineData->SMA240 = klineObject->TryGetField(TEXT("SMA240"))->AsNumber();
							klineData->SMA240SUM = klineObject->TryGetField(TEXT("SMA240SUM"))->AsNumber();
							klineData->EMA5 = klineObject->TryGetField(TEXT("EMA5"))->AsNumber();
							klineData->EMA10 = klineObject->TryGetField(TEXT("EMA10"))->AsNumber();
							klineData->EMA20 = klineObject->TryGetField(TEXT("EMA20"))->AsNumber();
							klineData->EMA60 = klineObject->TryGetField(TEXT("EMA60"))->AsNumber();
							klineData->EMA240 = klineObject->TryGetField(TEXT("EMA240"))->AsNumber();
							klineData->DIF1 = klineObject->TryGetField(TEXT("DIF1"))->AsNumber();
							klineData->DIF2 = klineObject->TryGetField(TEXT("DIF2"))->AsNumber();
							klineData->DIF = klineObject->TryGetField(TEXT("DIF"))->AsNumber();
							klineData->DEA = klineObject->TryGetField(TEXT("DEA"))->AsNumber();
							klineData->MACD = klineObject->TryGetField(TEXT("MACD"))->AsNumber();
							klineData->BollUpper = klineObject->TryGetField(TEXT("BollUpper"))->AsNumber();
							klineData->BollLower = klineObject->TryGetField(TEXT("BollLower"))->AsNumber();
							klineData->KDJ_RSV = klineObject->TryGetField(TEXT("KDJ_RSV"))->AsNumber();
							klineData->KDJ_K = klineObject->TryGetField(TEXT("KDJ_K"))->AsNumber();
							klineData->KDJ_D = klineObject->TryGetField(TEXT("KDJ_D"))->AsNumber();
							klineData->KDJ_J = klineObject->TryGetField(TEXT("KDJ_J"))->AsNumber();
							klineData->RSI0_AVGUp = klineObject->TryGetField(TEXT("RSI0_AVGUp"))->AsNumber();
							klineData->RSI0_AVGDown = klineObject->TryGetField(TEXT("RSI0_AVGDown"))->AsNumber();
							klineData->RSI0 = klineObject->TryGetField(TEXT("RSI0"))->AsNumber();
							klineData->RSI1_AVGUp = klineObject->TryGetField(TEXT("RSI1_AVGUp"))->AsNumber();
							klineData->RSI1_AVGDown = klineObject->TryGetField(TEXT("RSI1_AVGDown"))->AsNumber();
							klineData->RSI1 = klineObject->TryGetField(TEXT("RSI1"))->AsNumber();
							klineData->RSI2_AVGUp = klineObject->TryGetField(TEXT("RSI2_AVGUp"))->AsNumber();
							klineData->RSI2_AVGDown = klineObject->TryGetField(TEXT("RSI2_AVGDown"))->AsNumber();
							klineData->RSI2 = klineObject->TryGetField(TEXT("RSI2"))->AsNumber();
							klineData->WR1 = klineObject->TryGetField(TEXT("WR1"))->AsNumber();
							klineData->WR2 = klineObject->TryGetField(TEXT("WR2"))->AsNumber();
							klineData->TR_Average = klineObject->TryGetField(TEXT("TR_Average"))->AsNumber();
							klineData->PDI_Average = klineObject->TryGetField(TEXT("PDI_Average"))->AsNumber();
							klineData->NDI_Average = klineObject->TryGetField(TEXT("NDI_Average"))->AsNumber();
							klineData->PDI = klineObject->TryGetField(TEXT("PDI"))->AsNumber();
							klineData->NDI = klineObject->TryGetField(TEXT("NDI"))->AsNumber();
							klineData->DX = klineObject->TryGetField(TEXT("DX"))->AsNumber();
							klineData->ADX = klineObject->TryGetField(TEXT("ADX"))->AsNumber();
							klineData->ADXR = klineObject->TryGetField(TEXT("ADXR"))->AsNumber();
							klineData->CCI_TP = klineObject->TryGetField(TEXT("CCI_TP"))->AsNumber();
							klineData->CCI_TPSUM = klineObject->TryGetField(TEXT("CCI_TPSUM"))->AsNumber();
							klineData->CCI_SMA = klineObject->TryGetField(TEXT("CCI_SMA"))->AsNumber();
							klineData->CCI_MAD = klineObject->TryGetField(TEXT("CCI_MAD"))->AsNumber();
							klineData->CCI = klineObject->TryGetField(TEXT("CCI"))->AsNumber();
							klineData->BIAS0_SMASUM = klineObject->TryGetField(TEXT("BIAS0_SMASUM"))->AsNumber();
							klineData->BIAS1_SMASUM = klineObject->TryGetField(TEXT("BIAS1_SMASUM"))->AsNumber();
							klineData->BIAS2_SMASUM = klineObject->TryGetField(TEXT("BIAS2_SMASUM"))->AsNumber();
							klineData->BIAS0 = klineObject->TryGetField(TEXT("BIAS0"))->AsNumber();
							klineData->BIAS1 = klineObject->TryGetField(TEXT("BIAS1"))->AsNumber();
							klineData->BIAS2 = klineObject->TryGetField(TEXT("BIAS2"))->AsNumber();
							klineData->HistoryVolumeRatio = klineObject->TryGetField(TEXT("HistoryVolumeRatio"))->AsNumber();
							klineData->VolumeRatio = klineObject->TryGetField(TEXT("VolumeRatio"))->AsNumber();
							klineData->VolumeSUM = klineObject->TryGetField(TEXT("VolumeSUM"))->AsNumber();
							outKLineDatas_.Add(klineData);
						}
					}
				}
			}
		}
		return true;
}

TSharedPtr<FQTStockListRow> UCompanyNameIndexWidget::GetFQTStockListRowByCodeOrName(FString innameorcode, int16 stockOrFund) {
	TSharedPtr<FQTStockListRow> forReturn;
	if (stockOrFund) {
		TSharedPtr<FQTStockListRow>* sourcecodeptr = FundRowListMap_.Find(innameorcode);
		if (sourcecodeptr)forReturn = *sourcecodeptr;
	}
	else {
		TSharedPtr<FQTStockListRow>* sourcecodeptr = StockRowListMap_.Find(innameorcode);
		if (sourcecodeptr)forReturn = *sourcecodeptr;
	} 
	return forReturn;
}

bool UCompanyNameIndexWidget::CheckStockCodeOrNameExist(const FString& stockCodeOrName){
	if (StockRowListMap_.Contains(stockCodeOrName) || FundRowListMap_.Contains(stockCodeOrName)) {
		return true;
	}
	return false;
}

void UCompanyNameIndexWidget::UpdateLatestDayLine(const FQTStockRealTimeData& latestDayLineData){
	TSharedPtr < FQTStockIndex >& lastestDayKLineData = outKLineDatas_.Last();
	if (lastestDayKLineData->Date != FDateTime::Now().GetYear() * 10000 + FDateTime::Now().GetMonth() * 100 + FDateTime::Now().GetDay()) {
		UE_LOG(LogTemp, Warning, TEXT("---------->> 最新日线数据的日期与当前日期不符,无法更新!"));
		return;
	}
	lastestDayKLineData->Open = latestDayLineData.OpenPrice;
	lastestDayKLineData->Close = latestDayLineData.LatestPrice;
	lastestDayKLineData->High = latestDayLineData.HighestPrice;
	lastestDayKLineData->Low = latestDayLineData.LowestPrice;
	lastestDayKLineData->Volume = 10000.0f * latestDayLineData.Volume;//FQTStockRealTimeData实时数据中的Volume单位是万手,而FQTStockIndex日线数据中的Volume单位是股,所以需要乘以10000进行转换
	lastestDayKLineData->Turnover = latestDayLineData.Turnover;
	lastestDayKLineData->Change = latestDayLineData.ChangeAmount;
	lastestDayKLineData->ChangeRatio = latestDayLineData.ChangeRatio;
	lastestDayKLineData->PriceRange = latestDayLineData.PriceRange;
	lastestDayKLineData->TurnoverRate = latestDayLineData.TurnoverRate;
	//存储更新后的日线数据到本地文件
	FString outputString;
	TSharedPtr<FJsonObject> rootObj = MakeShareable(new FJsonObject());
	rootObj->SetNumberField(TEXT("FetchedAt"), FDateTime::Now().GetYear() * 10000 + FDateTime::Now().GetMonth() * 100 + FDateTime::Now().GetDay());
	TArray<TSharedPtr<FJsonValue>> klineJsonArray;
	for (const auto& item : outKLineDatas_) {
		TSharedPtr<FJsonObject> klineObj = MakeShareable(new FJsonObject());
		klineObj->SetNumberField(TEXT("Date"), item->Date);
		klineObj->SetNumberField(TEXT("Open"), item->Open);
		klineObj->SetNumberField(TEXT("Close"), item->Close);
		klineObj->SetNumberField(TEXT("High"), item->High);
		klineObj->SetNumberField(TEXT("Low"), item->Low);
		klineObj->SetNumberField(TEXT("Change"), item->Change);
		klineObj->SetNumberField(TEXT("ChangeRatio"), item->ChangeRatio);
		klineObj->SetNumberField(TEXT("Volume"), item->Volume);
		klineObj->SetNumberField(TEXT("Turnover"), item->Turnover);
		klineObj->SetNumberField(TEXT("PriceRange"), item->PriceRange);
		klineObj->SetNumberField(TEXT("TurnoverRate"), item->TurnoverRate);
		klineJsonArray.Add(MakeShareable(new FJsonValueObject(klineObj)));
	}
	rootObj->SetArrayField(TEXT("Klines"), klineJsonArray);
	TSharedRef<TJsonWriter<>> jsonWriter = TJsonWriterFactory<>::Create(&outputString);
	if (FJsonSerializer::Serialize(rootObj.ToSharedRef(), jsonWriter)) {
		if (FFileHelper::SaveStringToFile(outputString, *currentFilename_)) { UE_LOG(LogTemp, Warning, TEXT("---------->> 成功更新最新日线数据到本地文件!")); }
		else UE_LOG(LogTemp, Error, TEXT("---------->> 保存最新日线数据到本地文件失败!"));
	}
}

void UCompanyNameIndexWidget::NativePreConstruct() {
	Super::NativePreConstruct(); 
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

	FetchStockListData();//从网上或本地获取股票列表数据
	FetchFundListData();//从网上或本地获取基金列表数据
	stockMonitor_ = nullptr;
}

void UCompanyNameIndexWidget::NativeConstruct(){
	Super::NativeConstruct();
	stockMonitor_ = NewObject<UStockMonitor>(this);
	if (stockMonitor_) {
		stockMonitor_->SetTimeOut(10.0f);
		stockMonitor_->SetRetryCount(3);
		stockMonitor_->SetCallbacks(
			FOnRequestSuccess::CreateUObject(this, &UCompanyNameIndexWidget::HandleStockDataResponse),
			FOnRequestSuccessF10::CreateUObject(this, &UCompanyNameIndexWidget::HandleF10DataResponse),
			FOnRequestFailed::CreateUObject(this, &UCompanyNameIndexWidget::HandleStockDataError)
		);
	}
}

void UCompanyNameIndexWidget::NativeDestruct(){
	Super::NativeDestruct();
	
}

FString UCompanyNameIndexWidget::GetKLineDataURL(const FString& StockCode, int inklt, int  infqt) {
	FString marketPrefix = TEXT("1"); // 默认沪市
	if (StockCode.StartsWith("0") || StockCode.StartsWith("2") || StockCode.StartsWith("3")) { marketPrefix = TEXT("0"); } // 深市
	return FString::Printf(
		TEXT("https://push2his.eastmoney.com/api/qt/stock/kline/get")
		TEXT("?secid=%s.%s")
		TEXT("&fields1=f1,f2,f3,f4,f5,f6")
		TEXT("&fields2=f51,f52,f53,f54,f55,f56,f57,f58,f59,f60,f61")
		TEXT("&klt=%d")                        // 日线
		TEXT("&fqt=%d")                         // 复权类型
		TEXT("&beg=20000101")            // 开始日期(默认从2000年1月1日到今天)
		TEXT("&end=20500101")
		TEXT("&lmt=1000")	                         // 最大返回1000条数据
		TEXT("&_=%lld"),
		*marketPrefix,
		*StockCode,
		inklt,
		infqt,
		FDateTime::Now().GetTicks());
}

bool UCompanyNameIndexWidget::ParseKLineDataResponse(const FString& ResponseString, TArray<TSharedPtr<FQTStockIndex>>& outKLineDatas){
	TSharedPtr<FJsonObject> jsonObject;
	TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(ResponseString);
	if (FJsonSerializer::Deserialize(jsonReader, jsonObject) && jsonObject.IsValid()) {
		TSharedPtr<FJsonObject> dataObject = jsonObject->GetObjectField(TEXT("data"));
		if (dataObject.IsValid()) {
			const TArray<TSharedPtr<FJsonValue>>* klinesArray;
			FString stockCode, stockName;
			if (dataObject->TryGetArrayField(TEXT("klines"), klinesArray) && dataObject->TryGetStringField(TEXT("code"), stockCode) && dataObject->TryGetStringField(TEXT("name"), stockName)) {
				outKLineDatas.Empty();//清空之前的数据
				for (auto& eachKLineValue : *klinesArray) {
					FString klineString = eachKLineValue->AsString();
					TArray<FString> klineElements;
					klineString.ParseIntoArray(klineElements, TEXT(","), true);
					if (klineElements.Num() >= 10) {
						TSharedPtr<FQTStockIndex>klineData = MakeShareable(new FQTStockIndex());
						klineData->IndexCode = stockCode;
						klineData->IndexChineseName = stockName;
						TArray<FString> datearray;
						klineElements[0].ParseIntoArray(datearray, TEXT("-"), true);
						klineData->Date = FCString::Atoi(*datearray[0]) * 10000 + FCString::Atoi(*datearray[1]) * 100 + FCString::Atoi(*datearray[2]);//日期转换成整数格式YYYYMMDD
						klineData->Open = FCString::Atof(*klineElements[1]);
						klineData->Close = FCString::Atof(*klineElements[2]);
						klineData->High = FCString::Atof(*klineElements[3]);
						klineData->Low = FCString::Atof(*klineElements[4]);
						klineData->Volume = FCString::Atof(*klineElements[5]);
						klineData->Turnover = FCString::Atof(*klineElements[6]);
						klineData->PriceRange = FCString::Atof(*klineElements[7]);
						klineData->ChangeRatio = FCString::Atof(*klineElements[8]);
						klineData->Change = FCString::Atof(*klineElements[9]);
						klineData->TurnoverRate = FCString::Atof(*klineElements[10]);
						outKLineDatas.Add(klineData);
					}
					else { UE_LOG(LogTemp, Error, TEXT("---------->> K线数据格式错误: %s"), *klineString); return false; }
				}
				return true;
			}
			else UE_LOG(LogTemp, Error, TEXT("---------->> klines数据字段缺失!"));
		}
		else UE_LOG(LogTemp, Error, TEXT("---------->> data数据字段缺失!"));
	}
	else UE_LOG(LogTemp, Error, TEXT("---------->> 解析股票日线响应数据失败!"));
	return false;
}

bool UCompanyNameIndexWidget::ParseStockListDataResponse(const FString& ResponseString, TMap<FString, TSharedPtr<FQTStockListRow>>& outStockListMap){
	TSharedPtr<FJsonValue>jsonValue;
	TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(ResponseString);
	if (FJsonSerializer::Deserialize(jsonReader, jsonValue) && jsonValue.IsValid()) {
		UE_LOG(LogTemp, Warning, TEXT("---------->> 解析股票列表响应数据成功!"));
		TArray< TSharedPtr < FJsonValue >> jsonValueArray = jsonValue->AsArray();
		for (auto& eachDiffValue : jsonValueArray) {
			TSharedPtr<FJsonObject> diffObject = eachDiffValue->AsObject();
			if (diffObject.IsValid()) {
				TSharedPtr<FQTStockListRow> stockListRow = MakeShareable(new FQTStockListRow());
				stockListRow->CODE = diffObject->TryGetField(TEXT("dm"))->AsString().Left(6);
				stockListRow->NAME = diffObject->TryGetField(TEXT("mc"))->AsString();
				stockListRow->CODEMARK = diffObject->TryGetField(TEXT("dm"))->AsString();
				stockListRow->NAMECODE = stockListRow->NAME + stockListRow->CODE;
				stockListRow->FUNDTYPE = TEXT("股票");
				if (stockListRow->CODE.StartsWith(TEXT("60"))) { stockListRow->MARK = TEXT("上海主板"); }
				else if (stockListRow->CODE.StartsWith(TEXT("00"))) { stockListRow->MARK = TEXT("深圳主板"); }
				else if (stockListRow->CODE.StartsWith(TEXT("30"))) { stockListRow->MARK = TEXT("深创业板"); }
				else if (stockListRow->CODE.StartsWith(TEXT("68"))) { stockListRow->MARK = TEXT("沪科创板"); }
				else if (stockListRow->CODE.StartsWith(TEXT("90")) || stockListRow->CODE.StartsWith(TEXT("91")) || stockListRow->CODE.StartsWith(TEXT("93"))) { stockListRow->MARK = TEXT("北交所"); }
				else { stockListRow->MARK = TEXT("其他市场"); }
				outStockListMap.Add(stockListRow->CODE, stockListRow);
				outStockListMap.Add(stockListRow->NAME, stockListRow);
			}
		}
		return true;
	}
	else UE_LOG(LogTemp, Error, TEXT("---------->> 解析股票列表响应数据失败!"));
	return false;
}

bool UCompanyNameIndexWidget::ParseFundListDataResponse(const FString& ResponseString, TMap<FString, TSharedPtr<FQTStockListRow>>& outFundListMap) {
	FString leftStr, rightStr;
	ResponseString.Split(TEXT("[["), &leftStr, &rightStr);
	FString contentStr, discardStr;
	rightStr.Split(TEXT("]]"), &contentStr, &discardStr);
	TArray<FString> fundsStr;
	contentStr.ParseIntoArray(fundsStr, TEXT("],["));
	if (fundsStr.Num() > 0) {
		for (auto& item : fundsStr) {
			TArray<FString> tempstrs;
			item.ParseIntoArray(tempstrs, TEXT(","));
			TSharedPtr<FQTStockListRow> fundListRow = MakeShareable(new FQTStockListRow());
			tempstrs[0].RemoveFromStart(TEXT("\""));
			tempstrs[0].RemoveFromEnd(TEXT("\""));
			tempstrs[2].RemoveFromStart(TEXT("\""));
			tempstrs[2].RemoveFromEnd(TEXT("\""));
			tempstrs[3].RemoveFromStart(TEXT("\""));
			tempstrs[3].RemoveFromEnd(TEXT("\""));
			fundListRow->CODE = tempstrs[0];
			fundListRow->NAME = tempstrs[2];
			fundListRow->NAMECODE = tempstrs[2] + tempstrs[0];
			fundListRow->FUNDTYPE = tempstrs[3];
			if (fundListRow->CODE.StartsWith(TEXT("15")) || fundListRow->CODE.StartsWith(TEXT("16")) || fundListRow->CODE.StartsWith(TEXT("18")))fundListRow->MARK = TEXT("深交所");
			else if (fundListRow->CODE.StartsWith(TEXT("50")) || fundListRow->CODE.StartsWith(TEXT("51")) || fundListRow->CODE.StartsWith(TEXT("52")))fundListRow->MARK = TEXT("上交所");
			else fundListRow->MARK = TEXT("其他市场");
			outFundListMap.Add(fundListRow->CODE, fundListRow);
			outFundListMap.Add(fundListRow->NAME, fundListRow);
		}
		return true;
	}
	return false;
}

void UCompanyNameIndexWidget::OnKLineDataRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful){
	if (bWasSuccessful && Response.IsValid()) {
		FString responseString = Response->GetContentAsString();
		outKLineDatas_.Empty();//清空之前的数据
		if (ParseKLineDataResponse(responseString, outKLineDatas_)) {
			//通知主画布数据已更新
			if (outKLineDatas_.Num() > 0 && mainCanvas)	mainCanvas->OnCompanyCommitted(outKLineDatas_);
			//将数据保存到本地文件
			UE_LOG(LogTemp, Warning, TEXT("---------->> 股票日线数据获取成功,技术指标计算完毕,正在保存到本地文件..."));
			TSharedPtr<FJsonObject> jsonObject = MakeShareable(new FJsonObject());
			TArray<TSharedPtr<FJsonValue>> klineArray;
			for (TSharedPtr<FQTStockIndex> eachKLineData : outKLineDatas_) {
				TSharedPtr<FJsonObject> klineObject = MakeShareable(new FJsonObject());
				klineObject->SetNumberField(TEXT("Date"), eachKLineData->Date);
				klineObject->SetNumberField(TEXT("Open"), eachKLineData->Open);
				klineObject->SetNumberField(TEXT("Close"), eachKLineData->Close);
				klineObject->SetNumberField(TEXT("High"), eachKLineData->High);
				klineObject->SetNumberField(TEXT("Low"), eachKLineData->Low);
				klineObject->SetNumberField(TEXT("Change"), eachKLineData->Change);
				klineObject->SetNumberField(TEXT("ChangeRatio"), eachKLineData->ChangeRatio);
				klineObject->SetNumberField(TEXT("Volume"), eachKLineData->Volume);
				klineObject->SetNumberField(TEXT("Turnover"), eachKLineData->Turnover);
				klineObject->SetNumberField(TEXT("PriceRange"), eachKLineData->PriceRange);
				klineObject->SetNumberField(TEXT("TurnoverRate"), eachKLineData->TurnoverRate);
				klineObject->SetNumberField(TEXT("ConsNumber"), eachKLineData->ConsNumber);
				klineObject->SetNumberField(TEXT("SMA5"), eachKLineData->SMA5);
				klineObject->SetNumberField(TEXT("SMA5SUM"), eachKLineData->SMA5SUM);
				klineObject->SetNumberField(TEXT("SMA10"), eachKLineData->SMA10);
				klineObject->SetNumberField(TEXT("SMA10SUM"), eachKLineData->SMA10SUM);
				klineObject->SetNumberField(TEXT("SMA20"), eachKLineData->SMA20);
				klineObject->SetNumberField(TEXT("SMA20SUM"), eachKLineData->SMA20SUM);
				klineObject->SetNumberField(TEXT("SMA60"), eachKLineData->SMA60);
				klineObject->SetNumberField(TEXT("SMA60SUM"), eachKLineData->SMA60SUM);
				klineObject->SetNumberField(TEXT("SMA240"), eachKLineData->SMA240);
				klineObject->SetNumberField(TEXT("SMA240SUM"), eachKLineData->SMA240SUM);
				klineObject->SetNumberField(TEXT("EMA5"), eachKLineData->EMA5);
				klineObject->SetNumberField(TEXT("EMA10"), eachKLineData->EMA10);
				klineObject->SetNumberField(TEXT("EMA20"), eachKLineData->EMA20);
				klineObject->SetNumberField(TEXT("EMA60"), eachKLineData->EMA60);
				klineObject->SetNumberField(TEXT("EMA240"), eachKLineData->EMA240);
				klineObject->SetNumberField(TEXT("BollUpper"), eachKLineData->BollUpper);
				klineObject->SetNumberField(TEXT("BollLower"), eachKLineData->BollLower);
				klineObject->SetNumberField(TEXT("DIF1"), eachKLineData->DIF1);
				klineObject->SetNumberField(TEXT("DIF2"), eachKLineData->DIF2);
				klineObject->SetNumberField(TEXT("DIF"), eachKLineData->DIF);
				klineObject->SetNumberField(TEXT("DEA"), eachKLineData->DEA);
				klineObject->SetNumberField(TEXT("MACD"), eachKLineData->MACD);
				klineObject->SetNumberField(TEXT("KDJ_RSV"), eachKLineData->KDJ_RSV);
				klineObject->SetNumberField(TEXT("KDJ_K"), eachKLineData->KDJ_K);
				klineObject->SetNumberField(TEXT("KDJ_D"), eachKLineData->KDJ_D);
				klineObject->SetNumberField(TEXT("KDJ_J"), eachKLineData->KDJ_J);
				klineObject->SetNumberField(TEXT("RSI0"), eachKLineData->RSI0);
				klineObject->SetNumberField(TEXT("RSI0_AVGUp"), eachKLineData->RSI0_AVGUp);
				klineObject->SetNumberField(TEXT("RSI0_AVGDown"), eachKLineData->RSI0_AVGDown);
				klineObject->SetNumberField(TEXT("RSI1"), eachKLineData->RSI1);
				klineObject->SetNumberField(TEXT("RSI1_AVGUp"), eachKLineData->RSI1_AVGUp);
				klineObject->SetNumberField(TEXT("RSI1_AVGDown"), eachKLineData->RSI1_AVGDown);
				klineObject->SetNumberField(TEXT("RSI2"), eachKLineData->RSI2);
				klineObject->SetNumberField(TEXT("RSI2_AVGUp"), eachKLineData->RSI2_AVGUp);
				klineObject->SetNumberField(TEXT("RSI2_AVGDown"), eachKLineData->RSI2_AVGDown);
				klineObject->SetNumberField(TEXT("WR1"), eachKLineData->WR1);
				klineObject->SetNumberField(TEXT("WR2"), eachKLineData->WR2);
				klineObject->SetNumberField(TEXT("TR_Average"), eachKLineData->TR_Average);
				klineObject->SetNumberField(TEXT("PDI_Average"), eachKLineData->PDI_Average);
				klineObject->SetNumberField(TEXT("NDI_Average"), eachKLineData->NDI_Average);
				klineObject->SetNumberField(TEXT("PDI"), eachKLineData->PDI);
				klineObject->SetNumberField(TEXT("NDI"), eachKLineData->NDI);
				klineObject->SetNumberField(TEXT("DX"), eachKLineData->DX);
				klineObject->SetNumberField(TEXT("ADX"), eachKLineData->ADX);
				klineObject->SetNumberField(TEXT("ADXR"), eachKLineData->ADXR);
				klineObject->SetNumberField(TEXT("CCI_TP"), eachKLineData->CCI_TP);
				klineObject->SetNumberField(TEXT("CCI_TPSUM"), eachKLineData->CCI_TPSUM);
				klineObject->SetNumberField(TEXT("CCI_SMA"), eachKLineData->CCI_SMA);
				klineObject->SetNumberField(TEXT("CCI_MAD"), eachKLineData->CCI_MAD);
				klineObject->SetNumberField(TEXT("CCI"), eachKLineData->CCI);
				klineObject->SetNumberField(TEXT("BIAS0_SMASUM"), eachKLineData->BIAS0_SMASUM);
				klineObject->SetNumberField(TEXT("BIAS1_SMASUM"), eachKLineData->BIAS1_SMASUM);
				klineObject->SetNumberField(TEXT("BIAS2_SMASUM"), eachKLineData->BIAS2_SMASUM);
				klineObject->SetNumberField(TEXT("BIAS0"), eachKLineData->BIAS0);
				klineObject->SetNumberField(TEXT("BIAS0_SMASUM"), eachKLineData->BIAS0_SMASUM);
				klineObject->SetNumberField(TEXT("BIAS1"), eachKLineData->BIAS1);
				klineObject->SetNumberField(TEXT("BIAS1_SMASUM"), eachKLineData->BIAS1_SMASUM);
				klineObject->SetNumberField(TEXT("BIAS2"), eachKLineData->BIAS2);
				klineObject->SetNumberField(TEXT("BIAS2_SMASUM"), eachKLineData->BIAS2_SMASUM);
				klineObject->SetNumberField(TEXT("HistoryVolumeRatio"), eachKLineData->HistoryVolumeRatio);
				klineObject->SetNumberField(TEXT("VolumeRatio"), eachKLineData->VolumeRatio);
				klineObject->SetNumberField(TEXT("VolumeSUM"), eachKLineData->VolumeSUM);
				klineObject->SetNumberField(TEXT("HistoryPEPercentile"), eachKLineData->HistoryPEPercentile);
				klineObject->SetNumberField(TEXT("FPE"), eachKLineData->FPE);
				klineObject->SetNumberField(TEXT("HistoryMaxPE"), eachKLineData->HistoryMaxPE);
				klineObject->SetNumberField(TEXT("HistoryMinPE"), eachKLineData->HistoryMinPE);
				klineObject->SetNumberField(TEXT("EPSJB"), eachKLineData->EPSJB);
				klineArray.Add(MakeShareable(new FJsonValueObject(klineObject)));
			}
			jsonObject->SetStringField(TEXT("Code"), outKLineDatas_.Last()->IndexCode);
			jsonObject->SetStringField(TEXT("Name"), outKLineDatas_.Last()->IndexChineseName);
			jsonObject->SetNumberField(TEXT("FetchedAt"), FDateTime::Now().GetYear() * 10000 + FDateTime::Now().GetMonth() * 100 + FDateTime::Now().GetDay());
			jsonObject->SetArrayField(TEXT("Klines"), klineArray);
			FString outputString;
			TSharedRef<TJsonWriter<>> jsonWriter = TJsonWriterFactory<>::Create(&outputString);
			if (FJsonSerializer::Serialize(jsonObject.ToSharedRef(), jsonWriter)) {
				if (FFileHelper::SaveStringToFile(outputString, *currentFilename_)) {
					UE_LOG(LogTemp, Warning, TEXT("---------->> 股票日线数据已成功保存到本地文件: %s"), *currentFilename_);
				}
				else UE_LOG(LogTemp, Error, TEXT("---------->> 保存股票日线数据到本地文件失败: %s"), *currentFilename_);
			}
			else UE_LOG(LogTemp, Error, TEXT("---------->> 序列化股票日线数据失败!"));
		}
	}
	else UE_LOG(LogTemp, Error, TEXT("---------->> 获取股票日线数据请求失败!"));
}

void UCompanyNameIndexWidget::OnKLineDataRequestCompleteJustSave(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful){
	if (bWasSuccessful && Response.IsValid()) {
		FString responseString = Response->GetContentAsString();
		TArray<TSharedPtr<FQTStockIndex>> forSaveKLineDatas;
		if (ParseKLineDataResponse(responseString, forSaveKLineDatas)) {
			//计算并保存技术指标数据
			if (forSaveKLineDatas.Num() > 0 && mainCanvas)	mainCanvas->CaculateAndStoreIndicators(forSaveKLineDatas);
			//将数据保存到本地文件
			UE_LOG(LogTemp, Warning, TEXT("---------->> 股票日线数据获取成功,正在保存到本地文件..."));
			TSharedPtr<FJsonObject> jsonObject = MakeShareable(new FJsonObject());
			TArray<TSharedPtr<FJsonValue>> klineArray;
			for (TSharedPtr<FQTStockIndex> eachKLineData : forSaveKLineDatas) {
				TSharedPtr<FJsonObject> klineObject = MakeShareable(new FJsonObject());
				klineObject->SetNumberField(TEXT("Date"), eachKLineData->Date);
				klineObject->SetNumberField(TEXT("Open"), eachKLineData->Open);
				klineObject->SetNumberField(TEXT("Close"), eachKLineData->Close);
				klineObject->SetNumberField(TEXT("High"), eachKLineData->High);
				klineObject->SetNumberField(TEXT("Low"), eachKLineData->Low);
				klineObject->SetNumberField(TEXT("Change"), eachKLineData->Change);
				klineObject->SetNumberField(TEXT("ChangeRatio"), eachKLineData->ChangeRatio);
				klineObject->SetNumberField(TEXT("Volume"), eachKLineData->Volume);
				klineObject->SetNumberField(TEXT("Turnover"), eachKLineData->Turnover);
				klineObject->SetNumberField(TEXT("PriceRange"), eachKLineData->PriceRange);
				klineObject->SetNumberField(TEXT("TurnoverRate"), eachKLineData->TurnoverRate);
				klineObject->SetNumberField(TEXT("ConsNumber"), eachKLineData->ConsNumber);
				klineObject->SetNumberField(TEXT("SMA5"), eachKLineData->SMA5);
				klineObject->SetNumberField(TEXT("SMA5SUM"), eachKLineData->SMA5SUM);
				klineObject->SetNumberField(TEXT("SMA10"), eachKLineData->SMA10);
				klineObject->SetNumberField(TEXT("SMA10SUM"), eachKLineData->SMA10SUM);
				klineObject->SetNumberField(TEXT("SMA20"), eachKLineData->SMA20);
				klineObject->SetNumberField(TEXT("SMA20SUM"), eachKLineData->SMA20SUM);
				klineObject->SetNumberField(TEXT("SMA60"), eachKLineData->SMA60);
				klineObject->SetNumberField(TEXT("SMA60SUM"), eachKLineData->SMA60SUM);
				klineObject->SetNumberField(TEXT("SMA240"), eachKLineData->SMA240);
				klineObject->SetNumberField(TEXT("SMA240SUM"), eachKLineData->SMA240SUM);
				klineObject->SetNumberField(TEXT("EMA5"), eachKLineData->EMA5);
				klineObject->SetNumberField(TEXT("EMA10"), eachKLineData->EMA10);
				klineObject->SetNumberField(TEXT("EMA20"), eachKLineData->EMA20);
				klineObject->SetNumberField(TEXT("EMA60"), eachKLineData->EMA60);
				klineObject->SetNumberField(TEXT("EMA240"), eachKLineData->EMA240);
				klineObject->SetNumberField(TEXT("BollUpper"), eachKLineData->BollUpper);
				klineObject->SetNumberField(TEXT("BollLower"), eachKLineData->BollLower);
				klineObject->SetNumberField(TEXT("DIF1"), eachKLineData->DIF1);
				klineObject->SetNumberField(TEXT("DIF2"), eachKLineData->DIF2);
				klineObject->SetNumberField(TEXT("DIF"), eachKLineData->DIF);
				klineObject->SetNumberField(TEXT("DEA"), eachKLineData->DEA);
				klineObject->SetNumberField(TEXT("MACD"), eachKLineData->MACD);
				klineObject->SetNumberField(TEXT("KDJ_RSV"), eachKLineData->KDJ_RSV);
				klineObject->SetNumberField(TEXT("KDJ_K"), eachKLineData->KDJ_K);
				klineObject->SetNumberField(TEXT("KDJ_D"), eachKLineData->KDJ_D);
				klineObject->SetNumberField(TEXT("KDJ_J"), eachKLineData->KDJ_J);
				klineObject->SetNumberField(TEXT("RSI0"), eachKLineData->RSI0);
				klineObject->SetNumberField(TEXT("RSI0_AVGUp"), eachKLineData->RSI0_AVGUp);
				klineObject->SetNumberField(TEXT("RSI0_AVGDown"), eachKLineData->RSI0_AVGDown);
				klineObject->SetNumberField(TEXT("RSI1"), eachKLineData->RSI1);
				klineObject->SetNumberField(TEXT("RSI1_AVGUp"), eachKLineData->RSI1_AVGUp);
				klineObject->SetNumberField(TEXT("RSI1_AVGDown"), eachKLineData->RSI1_AVGDown);
				klineObject->SetNumberField(TEXT("RSI2"), eachKLineData->RSI2);
				klineObject->SetNumberField(TEXT("RSI2_AVGUp"), eachKLineData->RSI2_AVGUp);
				klineObject->SetNumberField(TEXT("RSI2_AVGDown"), eachKLineData->RSI2_AVGDown);
				klineObject->SetNumberField(TEXT("WR1"), eachKLineData->WR1);
				klineObject->SetNumberField(TEXT("WR2"), eachKLineData->WR2);
				klineObject->SetNumberField(TEXT("TR_Average"), eachKLineData->TR_Average);
				klineObject->SetNumberField(TEXT("PDI_Average"), eachKLineData->PDI_Average);
				klineObject->SetNumberField(TEXT("NDI_Average"), eachKLineData->NDI_Average);
				klineObject->SetNumberField(TEXT("PDI"), eachKLineData->PDI);
				klineObject->SetNumberField(TEXT("NDI"), eachKLineData->NDI);
				klineObject->SetNumberField(TEXT("DX"), eachKLineData->DX);
				klineObject->SetNumberField(TEXT("ADX"), eachKLineData->ADX);
				klineObject->SetNumberField(TEXT("ADXR"), eachKLineData->ADXR);
				klineObject->SetNumberField(TEXT("CCI_TP"), eachKLineData->CCI_TP);
				klineObject->SetNumberField(TEXT("CCI_TPSUM"), eachKLineData->CCI_TPSUM);
				klineObject->SetNumberField(TEXT("CCI_SMA"), eachKLineData->CCI_SMA);
				klineObject->SetNumberField(TEXT("CCI_MAD"), eachKLineData->CCI_MAD);
				klineObject->SetNumberField(TEXT("CCI"), eachKLineData->CCI);
				klineObject->SetNumberField(TEXT("BIAS0_SMASUM"), eachKLineData->BIAS0_SMASUM);
				klineObject->SetNumberField(TEXT("BIAS1_SMASUM"), eachKLineData->BIAS1_SMASUM);
				klineObject->SetNumberField(TEXT("BIAS2_SMASUM"), eachKLineData->BIAS2_SMASUM);
				klineObject->SetNumberField(TEXT("BIAS0"), eachKLineData->BIAS0);
				klineObject->SetNumberField(TEXT("BIAS0_SMASUM"), eachKLineData->BIAS0_SMASUM);
				klineObject->SetNumberField(TEXT("BIAS1"), eachKLineData->BIAS1);
				klineObject->SetNumberField(TEXT("BIAS1_SMASUM"), eachKLineData->BIAS1_SMASUM);
				klineObject->SetNumberField(TEXT("BIAS2"), eachKLineData->BIAS2);
				klineObject->SetNumberField(TEXT("BIAS2_SMASUM"), eachKLineData->BIAS2_SMASUM);
				klineObject->SetNumberField(TEXT("HistoryVolumeRatio"), eachKLineData->HistoryVolumeRatio);
				klineObject->SetNumberField(TEXT("VolumeRatio"), eachKLineData->VolumeRatio);
				klineObject->SetNumberField(TEXT("VolumeSUM"), eachKLineData->VolumeSUM);
				klineObject->SetNumberField(TEXT("HistoryPEPercentile"), eachKLineData->HistoryPEPercentile);
				klineObject->SetNumberField(TEXT("FPE"), eachKLineData->FPE);
				klineObject->SetNumberField(TEXT("HistoryMaxPE"), eachKLineData->HistoryMaxPE);
				klineObject->SetNumberField(TEXT("HistoryMinPE"), eachKLineData->HistoryMinPE);
				klineObject->SetNumberField(TEXT("EPSJB"), eachKLineData->EPSJB);
				klineArray.Add(MakeShareable(new FJsonValueObject(klineObject)));
			}
			jsonObject->SetStringField(TEXT("Code"), forSaveKLineDatas.Last()->IndexCode);
			jsonObject->SetStringField(TEXT("Name"), forSaveKLineDatas.Last()->IndexChineseName);
			jsonObject->SetNumberField(TEXT("FetchedAt"), FDateTime::Now().GetYear() * 10000 + FDateTime::Now().GetMonth() * 100 + FDateTime::Now().GetDay());
			jsonObject->SetArrayField(TEXT("Klines"), klineArray);
			FString outputString;
			TSharedRef<TJsonWriter<>> jsonWriter = TJsonWriterFactory<>::Create(&outputString);
			if (FJsonSerializer::Serialize(jsonObject.ToSharedRef(), jsonWriter)) {
				TSharedPtr<FQTStockListRow>  tempStockListRow = GetFQTStockListRowByCodeOrName(forSaveKLineDatas.Last()->IndexCode);//根据股票代码获取对应的本地文件路径
				FString klinePath = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/KlineDatas/%s/Kline101.json"), *(tempStockListRow->NAMECODE));
				if (FFileHelper::SaveStringToFile(outputString, *klinePath)) {
					UE_LOG(LogTemp, Warning, TEXT("---------->> 股票日线数据已成功保存到本地文件: %s"), *klinePath);
					if (onFetchKLineDataToSave.IsBound())onFetchKLineDataToSave.Broadcast(buyOrSell);
				}
				else UE_LOG(LogTemp, Error, TEXT("---------->> 保存股票日线数据到本地文件失败: %s"), *klinePath);
			}
			else UE_LOG(LogTemp, Error, TEXT("---------->> 序列化股票日线数据失败!"));
		}
	}
	else UE_LOG(LogTemp, Error, TEXT("---------->> 获取股票日线数据请求失败!"));
}

void UCompanyNameIndexWidget::OnStockListDataRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful){
	if (bWasSuccessful && Response.IsValid()) {
		FString responseString = Response->GetContentAsString();
		//将数据保存到本地文件
		FString stockListFilename = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/StockList.json"));
		if (ParseStockListDataResponse(responseString, StockRowListMap_)) {
			UE_LOG(LogTemp, Warning, TEXT("---------->> 股票列表数据获取成功,正在保存到本地文件..."));
			TSharedPtr<FJsonObject> jsonObject = MakeShareable(new FJsonObject());//根对象
			TArray<TSharedPtr<FJsonValue>> stockListArray;
			for(auto& eachRowPair : StockRowListMap_) {
				TSharedPtr<FQTStockListRow> rowData = eachRowPair.Value;
				//为了避免重复添加,只添加股票代码作为键的条目
				if (eachRowPair.Key == rowData->CODE) {
					TSharedPtr<FJsonObject> rowObject = MakeShareable(new FJsonObject());
					rowObject->SetStringField(TEXT("CODE"), rowData->CODE);
					rowObject->SetStringField(TEXT("NAME"), rowData->NAME);
					rowObject->SetStringField(TEXT("CODEMARK"), rowData->CODEMARK);
					rowObject->SetStringField(TEXT("NAMECODE"), rowData->NAMECODE);
					rowObject->SetStringField(TEXT("FUNDTYPE"), rowData->FUNDTYPE);
					rowObject->SetStringField(TEXT("MARK"), rowData->MARK);
					stockListArray.Add(MakeShareable(new FJsonValueObject(rowObject)));
				}
			}
			jsonObject->SetNumberField(TEXT("FetchedAt"), FDateTime::Now().GetYear() * 10000 + FDateTime::Now().GetMonth() * 100 + FDateTime::Now().GetDay());
			jsonObject->SetArrayField(TEXT("StockList"), stockListArray);
			FString outputString;
			TSharedRef<TJsonWriter<>> jsonWriter = TJsonWriterFactory<>::Create(&outputString);
			if (FJsonSerializer::Serialize(jsonObject.ToSharedRef(), jsonWriter)) {
				if (FFileHelper::SaveStringToFile(outputString, *stockListFilename)) {
					UE_LOG(LogTemp, Warning, TEXT("---------->> 股票列表数据已成功保存到本地文件: %s"), *stockListFilename);
				}
				else UE_LOG(LogTemp, Error, TEXT("---------->> 保存股票列表数据到本地文件失败: %s"), *stockListFilename);
			}
			else UE_LOG(LogTemp, Error, TEXT("---------->> 序列化股票列表数据失败!"));
		}
	}
	else UE_LOG(LogTemp, Error, TEXT("---------->> 获取股票列表数据请求失败!"));
}

void UCompanyNameIndexWidget::OnFundListDataRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
	if (bWasSuccessful && Response.IsValid()) {
		FString responseString = Response->GetContentAsString();
		//将数据保存到本地文件
		FString fundListFilename = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/FundList.json"));
		if (ParseFundListDataResponse(responseString, FundRowListMap_)) {
			UE_LOG(LogTemp, Warning, TEXT("---------->> 基金列表数据获取成功,正在保存到本地文件..."));
			TSharedPtr<FJsonObject> jsonObject = MakeShareable(new FJsonObject());//根对象
			TArray<TSharedPtr<FJsonValue>> fundListArray;
			for (auto& eachRowPair : FundRowListMap_) {
				TSharedPtr<FQTStockListRow> rowData = eachRowPair.Value;
				//为了避免重复添加,只添加基金代码作为键的条目
				if (eachRowPair.Key == rowData->CODE) {
					TSharedPtr<FJsonObject> rowObject = MakeShareable(new FJsonObject());
					rowObject->SetStringField(TEXT("CODE"), rowData->CODE);
					rowObject->SetStringField(TEXT("NAME"), rowData->NAME);
					rowObject->SetStringField(TEXT("CODEMARK"), rowData->CODEMARK);
					rowObject->SetStringField(TEXT("NAMECODE"), rowData->NAMECODE);
					rowObject->SetStringField(TEXT("FUNDTYPE"), rowData->FUNDTYPE);
					rowObject->SetStringField(TEXT("MARK"), rowData->MARK);
					fundListArray.Add(MakeShareable(new FJsonValueObject(rowObject)));
				}
			}
			jsonObject->SetNumberField(TEXT("FetchedAt"), FDateTime::Now().GetYear() * 10000 + FDateTime::Now().GetMonth() * 100 + FDateTime::Now().GetDay());
			jsonObject->SetArrayField(TEXT("FundList"), fundListArray);
			FString outputString;
			TSharedRef<TJsonWriter<>> jsonWriter = TJsonWriterFactory<>::Create(&outputString);
			if (FJsonSerializer::Serialize(jsonObject.ToSharedRef(), jsonWriter)) {
				if (FFileHelper::SaveStringToFile(outputString, *fundListFilename)) {
					UE_LOG(LogTemp, Warning, TEXT("---------->> 基金列表数据已成功保存到本地文件: %s"), *fundListFilename);
				}
				else UE_LOG(LogTemp, Error, TEXT("---------->> 保存基金列表数据到本地文件失败: %s"), *fundListFilename);
			}
			else UE_LOG(LogTemp, Error, TEXT("---------->> 序列化基金列表数据失败!"));
		}
	}
	else UE_LOG(LogTemp, Error, TEXT("---------->> 获取基金列表数据请求失败!"));
}

void UCompanyNameIndexWidget::OnIntroductionDataRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful){
	if (bWasSuccessful && Response.IsValid()) {
		FString responseString = Response->GetContentAsString();
		TSharedPtr<FJsonObject> rootObj;
		TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(responseString);
		if (FJsonSerializer::Deserialize(jsonReader, rootObj) && rootObj.IsValid()) {
			TSharedPtr < FJsonObject > resultObj = rootObj->TryGetField(TEXT("result"))->AsObject();
			if (resultObj.IsValid()) {
				TArray< TSharedPtr <FJsonValue>> datavalues = resultObj->TryGetField(TEXT("data"))->AsArray();
				if (!datavalues.IsEmpty()) {
					TSharedPtr<FJsonObject> jsonObj = datavalues[0]->AsObject();
					if (jsonObj.IsValid()) {
						TSharedPtr<FQTCompanyAbstractRow> currentCompanyIntroduction = MakeShareable(new FQTCompanyAbstractRow());
						currentCompanyIntroduction->CompanyName = jsonObj->GetStringField(TEXT("ORG_NAME"));
						currentCompanyIntroduction->EnglishName = jsonObj->GetStringField(TEXT("ORG_NAME_EN"));
						currentCompanyIntroduction->OldSimplifyName = jsonObj->GetStringField(TEXT("FORMERNAME"));
						currentCompanyIntroduction->IndexCodeA = jsonObj->GetStringField(TEXT("STR_CODEA"));
						currentCompanyIntroduction->SimplifyNameA = jsonObj->GetStringField(TEXT("STR_NAMEA"));
						currentCompanyIntroduction->IndexCodeB = jsonObj->GetStringField(TEXT("STR_CODEB"));
						currentCompanyIntroduction->SimplifyNameB = jsonObj->GetStringField(TEXT("STR_NAMEB"));
						currentCompanyIntroduction->IndexCodeH = jsonObj->GetStringField(TEXT("STR_CODEH"));
						currentCompanyIntroduction->SimplifyNameH = jsonObj->GetStringField(TEXT("STR_NAMEH"));
						currentCompanyIntroduction->MarketSegment = jsonObj->GetStringField(TEXT("TRADE_MARKET"));
						currentCompanyIntroduction->Industry = jsonObj->GetStringField(TEXT("INDUSTRYCSRC1"));
						currentCompanyIntroduction->LegalRepresentative = jsonObj->GetStringField(TEXT("LEGAL_PERSON"));
						currentCompanyIntroduction->RegisteredCapital = jsonObj->GetStringField(TEXT("REG_CAPITAL"));
						currentCompanyIntroduction->EstablishmentDate = jsonObj->GetStringField(TEXT("FOUND_DATE"));
						currentCompanyIntroduction->ListingDate = jsonObj->GetStringField(TEXT("LISTING_DATE"));
						currentCompanyIntroduction->OfficialWebsite = jsonObj->GetStringField(TEXT("ORG_WEB"));
						currentCompanyIntroduction->Email = jsonObj->GetStringField(TEXT("ORG_EMAIL"));
						currentCompanyIntroduction->Telephone = jsonObj->GetStringField(TEXT("ORG_TEL"));
						currentCompanyIntroduction->RegisteredAddress = jsonObj->GetStringField(TEXT("REG_ADDRESS"));
						currentCompanyIntroduction->OfficeAddress = jsonObj->GetStringField(TEXT("ADDRESS"));
						currentCompanyIntroduction->PostalCode = jsonObj->GetStringField(TEXT("ADDRESS_POSTCODE"));
						currentCompanyIntroduction->MainBusiness = jsonObj->GetStringField(TEXT("MAIN_BUSINESS"));
						currentCompanyIntroduction->ScopeOfBusiness = jsonObj->GetStringField(TEXT("BUSINESS_SCOPE"));
						currentCompanyIntroduction->OrganizationIntroduction = jsonObj->GetStringField(TEXT("ORG_PROFILE"));
						onIntroductionUpdate.Broadcast(*currentCompanyIntroduction);
						//将数据打上时间戳并保存到本地文件
						TSharedPtr<FJsonObject> writeRootObject = MakeShareable(new FJsonObject());
						writeRootObject->SetNumberField(TEXT("FetchedAt"), FDateTime::Now().GetYear() * 10000 + FDateTime::Now().GetMonth() * 100 + FDateTime::Now().GetDay());
						writeRootObject->SetObjectField(TEXT("CompanyIntroduction"), jsonObj);
						FString companyIntroductionPath = currentFilename_.Replace(TEXT("Kline101"), TEXT("Introduction"));
						FString outputString;
						TSharedRef<TJsonWriter<>> jsonWriter = TJsonWriterFactory<>::Create(&outputString);
						if (FJsonSerializer::Serialize(writeRootObject.ToSharedRef(), jsonWriter)) {
							if (FFileHelper::SaveStringToFile(outputString, *companyIntroductionPath)) {
								UE_LOG(LogTemp, Warning, TEXT("---------->> 公司简介数据已成功保存到本地文件: %s"), *companyIntroductionPath);
							}
							else UE_LOG(LogTemp, Error, TEXT("---------->> 保存公司简介数据到本地文件失败: %s"), *companyIntroductionPath);
						}
						else UE_LOG(LogTemp, Error, TEXT("---------->> 序列化公司简介数据失败!"));
					}
				}
			}
		}
		else UE_LOG(LogTemp, Error, TEXT("---------->> 公司简介数据反序列化失败!"));
	}
	else UE_LOG(LogTemp, Error, TEXT("---------->> 获取公司简介数据请求失败!"));
}

void UCompanyNameIndexWidget::FetchKLineData(const FString& StockCode, int inklt, int infqt){
	FString url = GetKLineDataURL(StockCode, inklt, infqt);
	SendHttpRequest(url);
}

void UCompanyNameIndexWidget::FetchStockListData(){
	FString fileContent;
	FString stockListFilename = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/StockList.json"));
	bool loadsuccesful = FFileHelper::LoadFileToString(fileContent, *stockListFilename);
	if (loadsuccesful) {//如果文件存在,就直接加载
		//检查文件的存储日期,如果超过一天没更新,就重新从网站获取股票列表数据并保存到本地文件
		UE_LOG(LogTemp, Warning, TEXT("---------->> 股票列表数据文件已存在,正在检查更新日期..."));
		TSharedPtr<FJsonObject> jsonObject;
		TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(fileContent);
		if (FJsonSerializer::Deserialize(jsonReader, jsonObject) && jsonObject.IsValid()) {
			int fetchedDate;
			jsonObject->TryGetNumberField(TEXT("FetchedAt"), fetchedDate);
			int currentDate = FDateTime::Now().GetYear() * 10000 + FDateTime::Now().GetMonth() * 100 + FDateTime::Now().GetDay();
			if (fetchedDate < currentDate) {//如果数据超过一天没更新,就重新从网站获取股票列表数据并保存到本地文件
				UE_LOG(LogTemp, Warning, TEXT("---------->> 股票列表数据文件超过一天未更新,正在重新从网站获取数据"));
				FString url = TEXT("https://api.zhituapi.com/hs/list/all?token=760A7515-632C-4624-A74C-5F3A26B8DDCD");//智兔网股票列表API
				TSharedRef<IHttpRequest, ESPMode::ThreadSafe> httpRequest = FHttpModule::Get().CreateRequest();
				httpRequest->OnProcessRequestComplete().BindUObject(this, &UCompanyNameIndexWidget::OnStockListDataRequestComplete);
				httpRequest->SetURL(url);
				httpRequest->SetVerb(TEXT("GET"));
				httpRequest->ProcessRequest();
			}
			else {
				UE_LOG(LogTemp, Warning, TEXT("---------->> 股票列表数据文件为最新版本,无需更新."));
				const TArray<TSharedPtr<FJsonValue>>* stockListArray;//股票
				if (jsonObject->TryGetArrayField(TEXT("StockList"), stockListArray)) {
					for (auto& eachStockValue : *stockListArray) {
						TSharedPtr<FJsonObject> stockObject = eachStockValue->AsObject();
						if (stockObject.IsValid()) {
							TSharedPtr<FQTStockListRow> stockListRow = MakeShareable(new FQTStockListRow());
							stockListRow->CODE = stockObject->TryGetField(TEXT("CODE"))->AsString();
							stockListRow->NAME = stockObject->TryGetField(TEXT("NAME"))->AsString();
							stockListRow->CODEMARK = stockObject->TryGetField(TEXT("CODEMARK"))->AsString();
							stockListRow->NAMECODE = stockObject->TryGetField(TEXT("NAMECODE"))->AsString();
							stockListRow->FUNDTYPE = stockObject->TryGetField(TEXT("FUNDTYPE"))->AsString();
							stockListRow->MARK = stockObject->TryGetField(TEXT("MARK"))->AsString();
							StockRowListMap_.Add(stockListRow->CODE, stockListRow);
							StockRowListMap_.Add(stockListRow->NAME, stockListRow);
						}
					}
				}
				else UE_LOG(LogTemp, Error, TEXT("---------->> StockList数据字段缺失!"));
			}
		}
		else UE_LOG(LogTemp, Error, TEXT("---------->> 解析股票列表数据文件失败!"));
	}
	else {//如果文件不存在,就从网站获取股票列表数据并保存到本地文件
		UE_LOG(LogTemp, Warning, TEXT("---------->> 股票列表数据文件不存在,正在从网站获取数据"));
		FString url = TEXT("https://api.zhituapi.com/hs/list/all?token=760A7515-632C-4624-A74C-5F3A26B8DDCD");//智兔网股票列表API
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> httpRequest = FHttpModule::Get().CreateRequest();
		httpRequest->OnProcessRequestComplete().BindUObject(this, &UCompanyNameIndexWidget::OnStockListDataRequestComplete);
		httpRequest->SetURL(url);
		httpRequest->SetVerb(TEXT("GET"));
		httpRequest->ProcessRequest();
	}
}

void UCompanyNameIndexWidget::FetchFundListData() {
	FString fileContent;
	FString fundListFilename = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/FundList.json"));
	bool loadsuccesful = FFileHelper::LoadFileToString(fileContent, *fundListFilename);
	if (loadsuccesful) {//如果文件存在,就直接加载
		//检查文件的存储日期,如果超过一天没更新,就重新从网站获取基金列表数据并保存到本地文件
		UE_LOG(LogTemp, Warning, TEXT("---------->> 基金列表数据文件已存在,正在检查更新日期..."));
		TSharedPtr<FJsonObject> jsonObject;
		TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(fileContent);
		if (FJsonSerializer::Deserialize(jsonReader, jsonObject) && jsonObject.IsValid()) {
			int fetchedDate;
			jsonObject->TryGetNumberField(TEXT("FetchedAt"), fetchedDate);
			int currentDate = FDateTime::Now().GetYear() * 10000 + FDateTime::Now().GetMonth() * 100 + FDateTime::Now().GetDay();
			if (fetchedDate < currentDate) {//如果数据超过一天没更新,就重新从网站获取基金列表数据并保存到本地文件
				UE_LOG(LogTemp, Warning, TEXT("---------->>基金列表数据文件超过一天未更新,正在重新从网站获取数据"));
				FString url = TEXT("http://fund.eastmoney.com/js/fundcode_search.js");//东方财富网基金列表数据API
				TSharedRef<IHttpRequest, ESPMode::ThreadSafe> httpRequest = FHttpModule::Get().CreateRequest();
				httpRequest->OnProcessRequestComplete().BindUObject(this, &UCompanyNameIndexWidget::OnFundListDataRequestComplete);
				httpRequest->SetURL(url);
				httpRequest->SetVerb(TEXT("GET"));
				httpRequest->ProcessRequest();
			}
			else {
				UE_LOG(LogTemp, Warning, TEXT("---------->> 基金列表数据文件为最新版本,无需更新."));
				const TArray<TSharedPtr<FJsonValue>>* fundListArray;//基金
				if (jsonObject->TryGetArrayField(TEXT("FundList"), fundListArray)) {
					for (auto& eachStockValue : *fundListArray) {
						TSharedPtr<FJsonObject> stockObject = eachStockValue->AsObject();
						if (stockObject.IsValid()) {
							TSharedPtr<FQTStockListRow> stockListRow = MakeShareable(new FQTStockListRow());
							TSharedPtr <FJsonValue> tempJsonValue = stockObject->TryGetField(TEXT("CODE"));
							if (tempJsonValue)stockListRow->CODE = tempJsonValue->AsString();
							tempJsonValue = stockObject->TryGetField(TEXT("NAME"));
							if (tempJsonValue)stockListRow->NAME = tempJsonValue->AsString();
							tempJsonValue = stockObject->TryGetField(TEXT("CODEMARK"));
							if (tempJsonValue)stockListRow->CODEMARK = tempJsonValue->AsString();
							tempJsonValue = stockObject->TryGetField(TEXT("NAMECODE"));
							if (tempJsonValue)stockListRow->NAMECODE = tempJsonValue->AsString();
							tempJsonValue = stockObject->TryGetField(TEXT("FUNDTYPE"));
							if (tempJsonValue)stockListRow->FUNDTYPE = tempJsonValue->AsString();
							tempJsonValue = stockObject->TryGetField(TEXT("MARK"));
							if (tempJsonValue)stockListRow->MARK = tempJsonValue->AsString();
							FundRowListMap_.Add(stockListRow->CODE, stockListRow);
							FundRowListMap_.Add(stockListRow->NAME, stockListRow);
						}
					}
				}
				else UE_LOG(LogTemp, Error, TEXT("---------->> FundList数据字段缺失!"));
			}
		}
		else UE_LOG(LogTemp, Error, TEXT("---------->> 解析基金列表数据文件失败!"));
	}
	else {//如果文件不存在,就从网站获取股票列表数据并保存到本地文件
		UE_LOG(LogTemp, Warning, TEXT("---------->> 基金列表数据文件不存在,正在从网站获取数据"));
		FString url = TEXT("http://fund.eastmoney.com/js/fundcode_search.js");//东方财富网基金列表数据API
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> httpRequest = FHttpModule::Get().CreateRequest();
		httpRequest->OnProcessRequestComplete().BindUObject(this, &UCompanyNameIndexWidget::OnFundListDataRequestComplete);
		httpRequest->SetURL(url);
		httpRequest->SetVerb(TEXT("GET"));
		httpRequest->ProcessRequest();
	}
}

void UCompanyNameIndexWidget::FetchCompanyIntroductionData(const FString& codeOrName){
	TSharedPtr < FQTStockListRow > tempstock = *(StockRowListMap_.Find(codeOrName));
	FString url = FString::Printf(TEXT("https://datacenter.eastmoney.com/securities/api/data/v1/get?reportName=RPT_F10_BASIC_ORGINFO&columns=ALL&quoteColumns=&filter=(SECUCODE%%3D%%22%s%%22)&pageNumber=1&pageSize=1&sortTypes=&sortColumns=&source=HSF10"), *tempstock->CODEMARK);
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> httpRequest = FHttpModule::Get().CreateRequest();
	httpRequest->OnProcessRequestComplete().BindUObject(this, &UCompanyNameIndexWidget::OnIntroductionDataRequestComplete);
	httpRequest->SetURL(url);
	httpRequest->SetVerb(TEXT("GET"));
	httpRequest->ProcessRequest();
}

void UCompanyNameIndexWidget::SendHttpRequest(FString url){
	//创建HTTP请求
	TSharedPtr<IHttpRequest> httpRequest = FHttpModule::Get().CreateRequest();
	if (!httpRequest.IsValid()) {
		UE_LOG(LogTemp, Error, TEXT("---------->> 创建HTTP请求失败!"));
		return;
	}
	//设置请求方法和URL
	httpRequest->SetURL(url);
	httpRequest->SetVerb(TEXT("GET"));
	//设置请求头
	for (const TPair<FString, FString>& header : requestHeaders_) {
		httpRequest->SetHeader(header.Key, header.Value);
	}
	//随机设置用户代理
	int32 randomIndex = FMath::RandRange(0, userAgentList_.Num() - 1);
	httpRequest->SetHeader(TEXT("User-Agent"), userAgentList_[randomIndex]);
	httpRequest->OnProcessRequestComplete().BindUObject(this, &UCompanyNameIndexWidget::OnKLineDataRequestComplete);
	//发送请求
	if (!httpRequest->ProcessRequest()) {
		UE_LOG(LogTemp, Error, TEXT("---------->> 发送HTTP请求失败! URL: %s"), *url);
		return;
	}
}

void UCompanyNameIndexWidget::SendHttpRequestJustSave(FString url){
	//创建HTTP请求
	TSharedPtr<IHttpRequest> httpRequest = FHttpModule::Get().CreateRequest();
	if (!httpRequest.IsValid()) {
		UE_LOG(LogTemp, Error, TEXT("---------->> 创建HTTP请求失败!"));
		return;
	}
	//设置请求方法和URL
	httpRequest->SetURL(url);
	httpRequest->SetVerb(TEXT("GET"));
	//设置请求头
	for (const TPair<FString, FString>& header : requestHeaders_) {
		httpRequest->SetHeader(header.Key, header.Value);
	}
	//随机设置用户代理
	int32 randomIndex = FMath::RandRange(0, userAgentList_.Num() - 1);
	httpRequest->SetHeader(TEXT("User-Agent"), userAgentList_[randomIndex]);
	httpRequest->OnProcessRequestComplete().BindUObject(this, &UCompanyNameIndexWidget::OnKLineDataRequestCompleteJustSave);
	//发送请求
	if (!httpRequest->ProcessRequest()) {
		UE_LOG(LogTemp, Error, TEXT("---------->> 发送HTTP请求失败! URL: %s"), *url);
		return;
	}
}

void UCompanyNameIndexWidget::GetRecentStockList(const FString& filename){
	FString fileContent;
	FString recentFilename = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/%s"), *filename);
	bool loadsuccesful = FFileHelper::LoadFileToString(fileContent, *recentFilename);
	if (loadsuccesful && stockListDownWidgetBP) {
		DownStockList_.Empty();
		stockListDownWidgetBP->ClearDownListItems();
		TSharedPtr<FJsonObject> rootObj;
		TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(fileContent);
		if (FJsonSerializer::Deserialize(jsonReader, rootObj) && rootObj.IsValid()) {
			TArray< TSharedPtr <FJsonValue>> recentStocks = rootObj->GetArrayField(TEXT("StockList"));
			if (recentStocks.IsEmpty()) { UE_LOG(LogTemp, Error, TEXT("---------->> %s的StockList为空!"), *filename); return; }
			for (auto& stock : recentStocks) {
				TSharedPtr<FJsonObject> jsonObj = stock->AsObject();
				TSharedPtr<FQTStockListRow>* sourcecodeptr = StockRowListMap_.Find(jsonObj->GetStringField(TEXT("CODE")));
				DownStockList_.Add(*sourcecodeptr);
			}
		}
		else UE_LOG(LogTemp, Error, TEXT("---------->> %s历史数据反序列化失败!"), *filename);
	}
	else UE_LOG(LogTemp, Error, TEXT("---------->> 读取%s历史失败! stockListDownWidgetBP 可能为空"), *filename);
}

void UCompanyNameIndexWidget::SaveRecentStockList(const FString& filename){
	//将数据打上时间戳并保存到本地文件
	TSharedPtr<FJsonObject> writeRootObject = MakeShareable(new FJsonObject());
	writeRootObject->SetNumberField(TEXT("FetchedAt"), FDateTime::Now().GetYear() * 10000 + FDateTime::Now().GetMonth() * 100 + FDateTime::Now().GetDay());
	TArray<TSharedPtr<FJsonValue>> jsonArray;
	for (auto& stock : DownStockList_) {
		TSharedPtr <FJsonObject> tempobj = MakeShareable(new FJsonObject());
		tempobj->SetStringField(TEXT("CODE"), stock->CODE);
		tempobj->SetStringField(TEXT("NAME"), stock->NAME);
		tempobj->SetStringField(TEXT("CODEMARK"), stock->CODEMARK);
		tempobj->SetStringField(TEXT("NAMECODE"), stock->NAMECODE);
		jsonArray.Add(MakeShared<FJsonValueObject>(tempobj));
	}
	writeRootObject->SetArrayField(TEXT("StockList"), jsonArray);
	FString recentFilename = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/%s"), *filename);
	FString outputString;
	TSharedRef<TJsonWriter<>> jsonWriter = TJsonWriterFactory<>::Create(&outputString);
	if (FJsonSerializer::Serialize(writeRootObject.ToSharedRef(), jsonWriter)) {
		if (FFileHelper::SaveStringToFile(outputString, *recentFilename)) {
			UE_LOG(LogTemp, Warning, TEXT("---------->> %s历史数据已成功保存到本地文件: %s"), *filename, *recentFilename);
		}
		else UE_LOG(LogTemp, Error, TEXT("---------->> 保存%s历史数据到本地文件失败: %s"), *filename, *recentFilename);
	}
	else UE_LOG(LogTemp, Error, TEXT("---------->> 序列化%s历史数据失败!"), *filename);
}

void UCompanyNameIndexWidget::FetchKLineDataJustSave(const FString& StockCode, int inklt, int infqt) {
	FString url = GetKLineDataURL(StockCode, inklt, infqt);
	SendHttpRequestJustSave(url);
}