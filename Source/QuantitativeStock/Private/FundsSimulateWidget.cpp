
#include "FundsSimulateWidget.h"
#include "CompanyNameIndexWidget.h"

void UFundsSimulateWidget::SimulateFunds(int inDate, float inBalance){
	CalculateRecommendedStocks(inDate, inBalance);//计算推荐股票列表
	//根据推荐股票列表和当前持仓计算出交易策略
	//先卖出再买入
	TArray<FString> sellStocks, buyStocks;
	for (auto& stockPair : RecommendSellStocks_) {
		if (stockPair.Value > 0)sellStocks.Add(stockPair.Key);
	}
	if (inBalance > 0.2f) {
		for (auto& stockPair : RecommendBuyStocks_) {
			if (stockPair.Value > 40) {//推荐权重大于40的加入推荐列表
				UE_LOG(LogTemp, Warning, TEXT("--------->>%s推荐值为%f,加入入手推荐列表"), *stockPair.Key, stockPair.Value);
				buyStocks.Add(stockPair.Key);
			}
		}
	}
	if (sellStocks.IsEmpty()) { UE_LOG(LogTemp, Warning, TEXT("--------->>%d出手推荐为空,今天没有适合卖出的股票"), inDate); }
	else {
		int firstOneThird = FMath::CeilToInt32(sellStocks.Num() * 0.3f);//将推荐出手的股票排前30%的卖掉
		TArray<FString> finalSellStocks;
		for (int i = 0; i < firstOneThird; ++i) {
			finalSellStocks.Add(sellStocks[i]);
		}
		UE_LOG(LogTemp, Warning, TEXT("--------->>%d推荐出手%d只股票"), inDate, firstOneThird);
		SellRecommendedStocks(finalSellStocks);//在蓝图里直接把这些股票卖掉就行了
	}
	if (buyStocks.IsEmpty()) { UE_LOG(LogTemp, Warning, TEXT("--------->>%d入手推荐为空,今天没有适合买入的股票"), inDate); }
	else {
		BuyRecommendedStocks(buyStocks);
	}
}

float UFundsSimulateWidget::GetStockClosePriceByDate(FString stockCode, int inDate){
	FQTStockIndex klineData;
	if (LoadStockKLineData(stockCode, inDate, klineData))	return klineData.Close;
	else {
		UE_LOG(LogTemp, Warning, TEXT("---------->> 加载K线数据失败,无法获取收盘价: %s"), *GetNameCode(stockCode));
	}
	return -1.0f;
}

int UFundsSimulateWidget::AddDaysToDate(int inDate, int daysToAdd){
	//简单的日期加法,没有考虑月份和年份的变化,只适用于同一个月内的日期计算
	int year = inDate / 10000;
	int month = (inDate % 10000) / 100;
	int day = inDate % 100;
	day += daysToAdd;
	//先判断年份是闰年还是平年,然后确定2月的天数
	int daysInMonth;
	if (month == 2) {
		bool isLeapYear = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
		daysInMonth = isLeapYear ? 29 : 28;
	}
	else if (month == 1 || month == 3 || month == 5 || month == 7 || month == 8 || month == 10 || month == 12) {
		daysInMonth = 31;
	}
	else {
		daysInMonth = 30;
	}
	if(day<=daysInMonth){
		return year * 10000 + month * 100 + day;
	}
	else {
		day -= daysInMonth;
		month += 1;
		if(month>12){
			month = 1;
			year += 1;
		}
	}
	return year * 10000 + month * 100 + day;
}

TArray<int> UFundsSimulateWidget::GetTradingDatesFromStartDate(int startDate){
	//加载任意一只股票的K线数据,从中提取出从startDate开始的交易日期列表
	TArray<FString>tempStocks;
	if (LoadListStocks(tempStocks) && tempStocks.Num() > 0) {
		FString fileContent;
		FString klinefilepath = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/KlineDatas/%s/Kline101.json"), *GetNameCode(tempStocks[0]));
		//加载K线数据
		if (FFileHelper::LoadFileToString(fileContent, *klinefilepath)) {
			TSharedPtr<FJsonObject> jsonObject;
			TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(fileContent);
			if (FJsonSerializer::Deserialize(jsonReader, jsonObject) && jsonObject.IsValid()) {
				const TArray<TSharedPtr<FJsonValue>>* klineArray;
				if (jsonObject->TryGetArrayField(TEXT("Klines"), klineArray)) {
					TArray<int> tradingDates;
					for(auto& klineValue : *klineArray){
						TSharedPtr<FJsonObject> klineObj = klineValue->AsObject();
						if (klineObj.IsValid()) {
							int32 KLineDate;
							klineObj->TryGetNumberField(TEXT("Date"), KLineDate);
							if (KLineDate >= startDate) {
								//添加到交易日期列表里
								tradingDates.Add(KLineDate);
							}
						}
					}
					return tradingDates;
				}
				else UE_LOG(LogTemp, Warning, TEXT("---------->> UFundsSimulateWidget::GetTradingDatesFromStartDate::K线数据文件缺少KLineDatas字段!"));
			}
			else UE_LOG(LogTemp, Warning, TEXT("---------->> UFundsSimulateWidget::GetTradingDatesFromStartDate::解析K线数据文件失败!"));
		}
		else UE_LOG(LogTemp, Warning, TEXT("---------->> UFundsSimulateWidget::GetTradingDatesFromStartDate::加载K线数据文件失败: %s"), *klinefilepath);
	}
	else { UE_LOG(LogTemp, Warning, TEXT("---------->> UFundsSimulateWidget::GetTradingDatesFromStartDate::获取最近访问股票列表失败!")); }
	return TArray<int>();
}

void UFundsSimulateWidget::CalculateRecommendedStocks(int inDate, float inBalance){
	//先根据持仓的股票计算出推荐卖出的股票列表(根据推荐权重,对RecommendSellStocks_进行降序排序即可)
	if (RecommendSellStocks_.Num() > 0) {
		for (auto& stockPair : RecommendSellStocks_) {
			FQTStockIndex klineData;
			if (LoadStockKLineData(stockPair.Key, inDate, klineData))	stockPair.Value = AnalyzeIndicatorsForSell(stockPair.Key, klineData);
			else UE_LOG(LogTemp, Warning, TEXT("---------->> 加载K线数据失败,无法计算卖出推荐权重: %s"), *GetNameCode(stockPair.Key));
		}
		//对RecommendSellStocks_进行降序排序
		RecommendSellStocks_.ValueSort([](float A, float B) {	return A > B; });
	}
	if (inBalance < 0.2f) { 
		UE_LOG(LogTemp, Warning, TEXT("---------->> 可用余额小于20%%,无法买入股票,入手推荐列表不用分析,清空RecommendBuyStocks_!"));
		RecommendBuyStocks_.Empty();
		return; 
	}
	TArray<FString> recentStocks;
	//加载最近的股票列表(用于计算推荐入手的股票)
	bool bloadsuccesful = LoadListStocks(recentStocks);
	if(bloadsuccesful){
		UE_LOG(LogTemp, Warning, TEXT("---------->> 成功加载最近的股票列表!"));
		for(auto&stock:recentStocks){
			FQTStockIndex klineData;
			FQTFinancialF10Main f10Data;
			if(LoadStockKLineData(stock,inDate,klineData) && LoadStockF10Data(stock,inDate,f10Data)){
				//根据K线数据和F10数据计算推荐权重
				float recommendWeight = AnalyzeF10AndIndicatorsForBuy(stock, klineData, f10Data);
				RecommendBuyStocks_.Add(stock, recommendWeight);
			}
			else UE_LOG(LogTemp, Warning, TEXT("---------->> 加载K线数据或F10数据失败,无法计算推荐权重: %s"), *GetNameCode(stock));
		}
		//对RecommendBuyStocks_进行降序排序
		RecommendBuyStocks_.ValueSort([](float A, float B) {	return A > B; });
	}
	else { UE_LOG(LogTemp, Warning, TEXT("---------->> 加载最近的股票列表失败!")); }
}

bool UFundsSimulateWidget::LoadStockKLineData(FString stockCode, int inDate, FQTStockIndex& outKLineData){
	FString fileContent;
	FString klinefilepath = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/KlineDatas/%s/Kline101.json"), *GetNameCode(stockCode));
	//加载K线数据
	if (FFileHelper::LoadFileToString(fileContent, *klinefilepath)) {
		TSharedPtr<FJsonObject> jsonObject;
		TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(fileContent);
		if (FJsonSerializer::Deserialize(jsonReader, jsonObject) && jsonObject.IsValid()) {
			const TArray<TSharedPtr<FJsonValue>>* klineArray;
			if (jsonObject->TryGetArrayField(TEXT("Klines"), klineArray)) {
				for (int i = klineArray->Num() - 1; i >= 0;--i) {
					TSharedPtr<FJsonObject> klineObj = (*klineArray)[i]->AsObject();
					if (klineObj.IsValid()) {
						int32 KLineDate;
						klineObj->TryGetNumberField(TEXT("Date"), KLineDate);
						if (KLineDate <= inDate) {
							outKLineData.Date = KLineDate;
							klineObj->TryGetNumberField(TEXT("Open"), outKLineData.Open);
							klineObj->TryGetNumberField(TEXT("High"), outKLineData.High);
							klineObj->TryGetNumberField(TEXT("Low"), outKLineData.Low);
							klineObj->TryGetNumberField(TEXT("Close"), outKLineData.Close);
							klineObj->TryGetNumberField(TEXT("Volume"), outKLineData.Volume);
							klineObj->TryGetNumberField(TEXT("HistoryVolumeRatio"), outKLineData.HistoryVolumeRatio);
							klineObj->TryGetNumberField(TEXT("MACD"), outKLineData.MACD);
							klineObj->TryGetNumberField(TEXT("KDJ_K"), outKLineData.KDJ_K);
							klineObj->TryGetNumberField(TEXT("KDJ_D"), outKLineData.KDJ_D);
							klineObj->TryGetNumberField(TEXT("KDJ_J"), outKLineData.KDJ_J);
							klineObj->TryGetNumberField(TEXT("RSI0"), outKLineData.RSI0);
							klineObj->TryGetNumberField(TEXT("RSI1"), outKLineData.RSI1);
							klineObj->TryGetNumberField(TEXT("RSI2"), outKLineData.RSI2);
							klineObj->TryGetNumberField(TEXT("WR1"), outKLineData.WR1);
							klineObj->TryGetNumberField(TEXT("WR2"), outKLineData.WR2);
							klineObj->TryGetNumberField(TEXT("PDI"), outKLineData.PDI);
							klineObj->TryGetNumberField(TEXT("NDI"), outKLineData.NDI);
							klineObj->TryGetNumberField(TEXT("ADX"), outKLineData.ADX);
							klineObj->TryGetNumberField(TEXT("CCI"), outKLineData.CCI);
							klineObj->TryGetNumberField(TEXT("BIAS0"), outKLineData.BIAS0);
							klineObj->TryGetNumberField(TEXT("BIAS1"), outKLineData.BIAS1);
							klineObj->TryGetNumberField(TEXT("BIAS2"), outKLineData.BIAS2);
							return true;
						}
					}
				}
			}
			else UE_LOG(LogTemp, Warning, TEXT("---------->> K线数据文件缺少KLineDatas字段!"));
		}
		else UE_LOG(LogTemp, Warning, TEXT("---------->> 解析K线数据文件失败!"));
	}
	else UE_LOG(LogTemp, Warning, TEXT("---------->> 加载K线数据文件失败: %s"), *klinefilepath);
	return false;
}

bool UFundsSimulateWidget::LoadStockF10Data(FString stockCode, int inDate, FQTFinancialF10Main& outF10Data){
	FString fileContent;
	FString f10filepath = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/KlineDatas/%s/F10.json"), *GetNameCode(stockCode));
	//加载F10数据
	if (FFileHelper::LoadFileToString(fileContent, *f10filepath)) {
		TSharedPtr<FJsonObject> jsonObject;
		TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(fileContent);
		if (FJsonSerializer::Deserialize(jsonReader, jsonObject) && jsonObject.IsValid()) {
			const TArray<TSharedPtr<FJsonValue>>* f10Array;
			if (jsonObject->TryGetArrayField(TEXT("F10"), f10Array)) {
				for (auto& f10Value : *f10Array) {
					TSharedPtr<FJsonObject> f10Obj = f10Value->AsObject();
					if (f10Obj.IsValid()) {
						int32 F10Date;
						f10Obj->TryGetNumberField(TEXT("REPORTDATE"), F10Date);
						if (F10Date <= inDate) {
							outF10Data.REPORTDATE = F10Date;
							f10Obj->TryGetStringField(TEXT("SECURITY_CODE"), outF10Data.SECURITY_CODE);
							f10Obj->TryGetNumberField(TEXT("ROEJQ"), outF10Data.ROEJQ);
							f10Obj->TryGetNumberField(TEXT("ZCFZL"), outF10Data.ZCFZL);
							f10Obj->TryGetNumberField(TEXT("MGJYXJJE"), outF10Data.MGJYXJJE);
							f10Obj->TryGetNumberField(TEXT("EPSJB"), outF10Data.EPSJB);
							return true;
						}
					}
				}
			}
			else UE_LOG(LogTemp, Warning, TEXT("---------->> F10数据文件缺少F10字段!"));
		}
		else UE_LOG(LogTemp, Warning, TEXT("---------->> 解析F10数据文件失败!"));
	}
	else UE_LOG(LogTemp, Warning, TEXT("---------->> 加载F10数据文件失败: %s"), *f10filepath);
	return false;
}

FString UFundsSimulateWidget::GetNameCode(FString stockCode) {
	if (companyNameIndexWidgetBP) {
		TSharedPtr<FQTStockListRow>  tempStockListRow = companyNameIndexWidgetBP->GetFQTStockListRowByCodeOrName(stockCode);//根据股票代码获取对应的本地文件路径
		return tempStockListRow->NAMECODE;
	}
	else { UE_LOG(LogTemp, Warning, TEXT("---------->> companyNameIndexWidgetBP未设置,无法获取股票名称代码: %s"), *stockCode); }
	return stockCode;//如果companyNameIndexWidgetBP没有设置成功,就直接返回股票代码
}

bool UFundsSimulateWidget::LoadListStocks(TArray<FString>& outStocks){
	FString stockListFilename = FPaths::ProjectDir() + FString("Saved/StockDatas/RecentStockList.json");
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
						if (stockName.StartsWith(TEXT("ST")) || stockName.StartsWith(TEXT("*ST"))) continue;//如果股票名称以过滤字符开头,就跳过这个股票
						outStocks.Add(stockCode);
					}
				}
				return true;
			}
			else { UE_LOG(LogTemp, Error, TEXT("---------->> StockList数据字段缺失!")); return false; }
		}
		else { UE_LOG(LogTemp, Error, TEXT("---------->> 解析股票列表数据文件失败!")); return false; }
	}
	else { UE_LOG(LogTemp, Error, TEXT("---------->> 加载股票列表数据文件失败!")); return false; }
	return false;
}

float UFundsSimulateWidget::AnalyzeIndicatorsForSell(FString instock, const FQTStockIndex& Indicators){
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
	StockIndicatorColors_.Add(instock, Colors);

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
	UE_LOG(LogTemp, Log, TEXT("=== %s 技术指标分析 ==="), *instock);
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
	float WeightAdjustment = URecommendStocksWidget::CalculateWeightForSell(OutRedLightCount, OutGreenLightCount);
	UE_LOG(LogTemp, Log, TEXT("权重调整: %.2f"), WeightAdjustment);

	return WeightAdjustment;
}

float UFundsSimulateWidget::AnalyzeF10AndIndicatorsForBuy(FString stockCode, const FQTStockIndex& Indicators, const FQTFinancialF10Main& inF10Data){
	UE_LOG(LogTemp, Warning, TEXT("AnalyzeF10AndIndicatorsForBuy:: Analyzing stock: %s"), *GetNameCode(stockCode));

	// ==================== 3. 财务指标筛选 ====================
	// ROE < 0 或 资产负债率 > 80 或 每股经营现金流 < 0，直接排除
	if (inF10Data.ROEJQ < 0 || inF10Data.ZCFZL > 80.0f || inF10Data.MGJYXJJE < 0) {
		UE_LOG(LogTemp, Warning, TEXT("%s 财务指标不合格: ROE=%.2f, 负债率=%.2f%%, 现金流=%.2f, 不推荐!"), *GetNameCode(stockCode), inF10Data.ROEJQ, inF10Data.ZCFZL, inF10Data.MGJYXJJE);
		return -100.0f;
	}

	// 计算基础权重
	float Weight = 0.0f;
	// 资产负债率 < 60% 优先
	if (inF10Data.ZCFZL < 60.0f) {
		float DebtBonus = (60.0f - inF10Data.ZCFZL) * 0.2f;
		Weight += DebtBonus;
		UE_LOG(LogTemp, Log, TEXT("%s 资产负债率%.2f%%, 加分%.2f"), *GetNameCode(stockCode), inF10Data.ZCFZL, DebtBonus);
	}
	// 每股经营现金流 > 每股收益 优先
	if (inF10Data.MGJYXJJE > inF10Data.EPSJB) {
		float CashBonus = (inF10Data.MGJYXJJE - inF10Data.EPSJB) * 0.5f;
		Weight += CashBonus;
		UE_LOG(LogTemp, Log, TEXT("%s 现金流%.2f > EPS%.2f, 加分%.2f"), *GetNameCode(stockCode), inF10Data.MGJYXJJE, inF10Data.EPSJB, CashBonus);
	}
	// ROE 越高越优先
	float ROEBonus = inF10Data.ROEJQ * 3.0f;
	Weight += ROEBonus;
	UE_LOG(LogTemp, Log, TEXT("%s ROE=%.2f%%, 加分%.2f"), *GetNameCode(stockCode), inF10Data.ROEJQ, ROEBonus);

	// 根据历史百分位调整权重
	if (Indicators.HistoryPEPercentile > 70.0f) {
		UE_LOG(LogTemp, Warning, TEXT("%s 市盈率历史百分位%.2f%%, 不推荐!"), *GetNameCode(stockCode), Indicators.HistoryPEPercentile);
		return -100.0f;
	}
	else if (Indicators.HistoryPEPercentile < 30.0f) {
		float PercentileBonus = (100.0f - Indicators.HistoryPEPercentile) * 0.2f;
		Weight += PercentileBonus;
		UE_LOG(LogTemp, Log, TEXT("%s 市盈率历史百分位低, 加分%.2f"), *GetNameCode(stockCode), PercentileBonus);
	}

	//主营业务分析(高科技板块的股票推荐权重增加,销售代理类型的公司推荐权重减小)
	FString businessDescription = GetBusinessDescription(stockCode);
	EBusinessCategory businessCategory;
	Weight += URecommendStocksWidget::AnalyzeBusinessAndGetWeightAdjustment(businessDescription, businessCategory);
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

FString UFundsSimulateWidget::GetBusinessDescription(FString stockCode) {
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