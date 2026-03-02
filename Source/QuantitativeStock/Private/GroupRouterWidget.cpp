// Fill out your copyright notice in the Description page of Project Settings.

#include "GroupRouterWidget.h"
#include "QTCurveVectorActor.h"
#include "StockListDownWidget.h"
#include "ItemRightClickWidget.h"
#include "CompanyNameIndexWidget.h"

bool UGroupRouterWidget::CopyItemToListPath(const FString& sourcePath,const FString& targetPath, int itemIndex, bool isCut){
	if (itemIndex < 0)return false;
	if (sourcePath == targetPath) { UE_LOG(LogTemp, Warning, TEXT("源路径和目标路径相同不操作")); return false; }
	TSharedPtr<FQTStockListRow> selectedItem = GetPathStockListItem(sourcePath, itemIndex, isCut);
	return SaveItemToListPath(targetPath, selectedItem);
}

TSharedPtr<FQTStockListRow> UGroupRouterWidget::GetPathStockListItem(const FString& inFileName , int16 inIndex, bool isCut){
	TSharedPtr<FQTStockListRow> outDownlistStock = MakeShareable(new FQTStockListRow());
	if (ListDownWidget_) {
		TArray<FString>currentcodes;
		ListDownWidget_->GetCurrentDownListDatas(currentcodes);
		if (currentcodes.IsEmpty()) { UE_LOG(LogTemp, Error, TEXT("---------->> 当前列表为空!"), *inFileName); return outDownlistStock; }
		if (currentcodes.IsValidIndex(inIndex)) {
			outDownlistStock = companyNameIndexWidget->GetFQTStockListRowByCodeOrName(currentcodes[inIndex]);
			if (isCut) {//剪切操作把指定的Item删掉
				ListDownWidget_->RemoveScrollBoxItem(inIndex);
			}
		}
	}
	return outDownlistStock;
}

bool UGroupRouterWidget::SaveItemToListPath(const FString& inFileName, TSharedPtr<FQTStockListRow>inItem){
	if (!inItem.IsValid() || inItem == nullptr)return false;
	FString fileContent;
	FString filename = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/%s"), *inFileName);
	bool loadsuccesful = FFileHelper::LoadFileToString(fileContent, *filename);
	if (!loadsuccesful)return false;
	TSharedPtr<FJsonObject> rootObj;
	TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(fileContent);
	TArray<TSharedPtr<FQTStockListRow>> downStockListItems;
	if (FJsonSerializer::Deserialize(jsonReader, rootObj) && rootObj.IsValid()) {
		TArray< TSharedPtr <FJsonValue>> downlistStocks = rootObj->GetArrayField(TEXT("StockList"));
		for (TSharedPtr <FJsonValue> tempitem : downlistStocks) {
			TSharedPtr<FQTStockListRow> outDownlistStock = MakeShareable(new FQTStockListRow());
			TSharedPtr<FJsonObject> jsonObj = tempitem->AsObject();
			TSharedPtr <FJsonValue> tempJsonValue = jsonObj->TryGetField(TEXT("CODE"));
			if(tempJsonValue)outDownlistStock->CODE = tempJsonValue->AsString();
			if (outDownlistStock->CODE == inItem->CODE) { UE_LOG(LogTemp, Warning, TEXT("目标路径里已有相同CODE的股票了,不添加了")); return false; }
			tempJsonValue = jsonObj->TryGetField(TEXT("NAME"));
			if (tempJsonValue)outDownlistStock->NAME = tempJsonValue->AsString();
			tempJsonValue = jsonObj->TryGetField(TEXT("CODEMARK"));
			if (tempJsonValue)outDownlistStock->CODEMARK = tempJsonValue->AsString();
			tempJsonValue = jsonObj->TryGetField(TEXT("NAMECODE"));
			if (tempJsonValue)outDownlistStock->NAMECODE = tempJsonValue->AsString();
			tempJsonValue = jsonObj->TryGetField(TEXT("FUNDTYPE"));
			if (tempJsonValue)outDownlistStock->FUNDTYPE = tempJsonValue->AsString();
			tempJsonValue = jsonObj->TryGetField(TEXT("MARK"));
			if (tempJsonValue)outDownlistStock->MARK = tempJsonValue->AsString();
			downStockListItems.Add(outDownlistStock);
		}
		downStockListItems.Add(inItem);//添加到队列最后
	}
	else { UE_LOG(LogTemp, Error, TEXT("---------->> %s历史数据反序列化失败!"), *inFileName); return false; }
	//读取成功,并添加成功,开始写入
	//将数据打上时间戳并保存到本地文件
	TSharedPtr<FJsonObject> writeRootObject = MakeShareable(new FJsonObject());
	writeRootObject->SetNumberField(TEXT("FetchedAt"), FDateTime::Now().GetYear() * 10000 + FDateTime::Now().GetMonth() * 100 + FDateTime::Now().GetDay());
	TArray<TSharedPtr<FJsonValue>> jsonArray;
	for (auto& stock : downStockListItems) {
		TSharedPtr <FJsonObject> tempobj = MakeShareable(new FJsonObject());
		tempobj->SetStringField(TEXT("CODE"), stock->CODE);
		tempobj->SetStringField(TEXT("NAME"), stock->NAME);
		tempobj->SetStringField(TEXT("CODEMARK"), stock->CODEMARK);
		tempobj->SetStringField(TEXT("NAMECODE"), stock->NAMECODE);
		tempobj->SetStringField(TEXT("FUNDTYPE"), stock->FUNDTYPE);
		tempobj->SetStringField(TEXT("MARK"), stock->MARK);
		jsonArray.Add(MakeShared<FJsonValueObject>(tempobj));
	}
	writeRootObject->SetArrayField(TEXT("StockList"), jsonArray);
	FString outputString;
	TSharedRef<TJsonWriter<>> jsonWriter = TJsonWriterFactory<>::Create(&outputString);
	if (FJsonSerializer::Serialize(writeRootObject.ToSharedRef(), jsonWriter)) {
		if (FFileHelper::SaveStringToFile(outputString, *filename)) {
			UE_LOG(LogTemp, Warning, TEXT("---------->> %s历史数据已成功保存到本地文件: %s"), *inFileName, *filename);
			return true;
		}
		else UE_LOG(LogTemp, Error, TEXT("---------->> 保存%s历史数据到本地文件失败: %s"), *inFileName, *filename);
	}
	else UE_LOG(LogTemp, Error, TEXT("---------->> 序列化%s历史数据失败!"), *inFileName);

	return false;
}
