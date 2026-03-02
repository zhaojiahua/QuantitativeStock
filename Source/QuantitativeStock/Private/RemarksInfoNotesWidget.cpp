
#include "RemarksInfoNotesWidget.h"

bool URemarksInfoNotesWidget::GetNotesFromFile(FString& outNotes){
	if (currentCodeName.IsEmpty()) {
		UE_LOG(LogTemp, Warning, TEXT("---------->> 无法加载备注信息: currentCodeName为空"));
		return false;
	}
	FString inFilePath = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/RemarksNotes/%s.json"), *currentCodeName);
	FString fileContent{};
	bool loadsuccuss = FFileHelper::LoadFileToString(fileContent, *inFilePath);
	if(loadsuccuss) {
		UE_LOG(LogTemp, Warning, TEXT("---------->> 之前保存的备注信息已成功加载: %s"), *inFilePath);
		TArray< TSharedPtr <FJsonValue>> rootArray;
		TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(fileContent);
		if (FJsonSerializer::Deserialize(jsonReader, rootArray)) {
			for(auto& item: rootArray) {
				TSharedPtr<FJsonObject> obj = item->AsObject();
				int date;
				obj->TryGetNumberField(TEXT("Date"), date);
				int monthday = date % 10000;
				FString dateStr = FString::Printf(TEXT("%d-%02d-%02d "), date / 10000, monthday / 100, monthday % 100);
				TArray< TSharedPtr <FJsonValue>> notes = obj->GetArrayField(TEXT("Notes"));
				for (int i = 0; i < notes.Num();++i) {
					TSharedPtr<FJsonObject> noteObj = notes[i]->AsObject();
					FString noteContent;
					noteObj->TryGetStringField(FString::FromInt(i + 1), noteContent);
					outNotes += FString::Printf(TEXT("\n·%s	%d.%s\n----------------------------------------------------------------------------------------------------"), *dateStr, i + 1, *noteContent);
				}
			}
			return true;
		}
		else { UE_LOG(LogTemp, Warning, TEXT("---------->> 备注信息反序列化失败!")); return false; }
	}
	else UE_LOG(LogTemp, Warning, TEXT("---------->> 备注信息加载失败: %s"), *inFilePath);
	return false;
}

bool URemarksInfoNotesWidget::SaveNotesToFile(const FString& inNotes) {
	if (currentCodeName.IsEmpty() || inNotes.IsEmpty()) {
		UE_LOG(LogTemp, Warning, TEXT("---------->> 无法保存备注信息: currentCodeName为空 || 备注信息为空"));
		return false;
	}
	//先从项目的Saved目录下加载之前保存的备注信息文件，如果存在的话
	FString noteFilePath = FPaths::ProjectDir() + FString::Printf(TEXT("Saved/StockDatas/RemarksNotes/%s.json"), *currentCodeName);
	FString fileContent{};
	bool loadsuccuss = FFileHelper::LoadFileToString(fileContent, *noteFilePath);
	TArray< TSharedPtr <FJsonValue>> rootArray;
	if (loadsuccuss) {//如果之前保存的备注信息文件存在，将其转化为Json对象，并将新的备注信息添加到其中
		UE_LOG(LogTemp, Warning, TEXT("---------->> 之前保存的备注信息已成功加载: %s"), *noteFilePath);
		/*
		备注信息文件的Json格式如下:
		[
			{
				"Date":20260301,
				"Notes":[{"1":"保存的备注信息1"},{"2":"保存的备注信息2"},{"3":"保存的备注信息3"}]
			},
			{
				"Date":20260228,
				"Notes":[{"1":"保存的备注信息1"},{"2":"保存的备注信息2"},{"3":"保存的备注信息3"}]
			}
		]
		*/
		TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(fileContent);
		if (FJsonSerializer::Deserialize(jsonReader, rootArray)) {
			//取rootArray的最后一个元素,如果它的Date字段等于当前日期,就将新的备注信息添加到它的Notes数组里;如果它的Date字段不等于当前日期,就创建一个新的Json对象,将新的备注信息添加到它的Notes数组里,并将这个新的Json对象添加到rootArray里
			TSharedPtr<FJsonObject> lastObj = rootArray.Last()->AsObject();
			int lastDate;
			lastObj->TryGetNumberField(TEXT("Date"), lastDate);
			int currentDate = FDateTime::Now().GetYear() * 10000 + FDateTime::Now().GetMonth() * 100 + FDateTime::Now().GetDay();
			if (lastDate == currentDate) {
				TArray< TSharedPtr <FJsonValue>> notesArray = lastObj->GetArrayField(TEXT("Notes"));
				TSharedPtr<FJsonObject> newNoteObj = MakeShareable(new FJsonObject());
				newNoteObj->SetStringField(FString::FromInt(notesArray.Num() + 1), inNotes);
				notesArray.Add(MakeShareable(new FJsonValueObject(newNoteObj)));
				lastObj->SetArrayField(TEXT("Notes"), notesArray);
			}
			else {
				TSharedPtr<FJsonObject> newDateObj = MakeShareable(new FJsonObject());
				newDateObj->SetNumberField(TEXT("Date"), currentDate);
				TArray< TSharedPtr <FJsonValue>> notesArray;
				TSharedPtr<FJsonObject> newNoteObj = MakeShareable(new FJsonObject());
				newNoteObj->SetStringField(TEXT("1"), inNotes);
				notesArray.Add(MakeShareable(new FJsonValueObject(newNoteObj)));
				newDateObj->SetArrayField(TEXT("Notes"), notesArray);
				rootArray.Add(MakeShareable(new FJsonValueObject(newDateObj)));
			}
		}
		else { UE_LOG(LogTemp, Warning, TEXT("---------->> 备注信息反序列化失败!")); return false; }
	}
	else {//如果之前保存的备注信息文件不存在,就创建一个新的Json对象,将新的备注信息添加到其中,并将这个新的Json对象添加到一个Json数组里,最后将这个Json数组转化为字符串,并保存到文件里
		UE_LOG(LogTemp, Warning, TEXT("---------->> 之前保存的备注信息文件不存在,正在创建新的备注信息文件: %s"), *noteFilePath);
		TSharedPtr<FJsonObject> newDateObj = MakeShareable(new FJsonObject());
		int currentDate = FDateTime::Now().GetYear() * 10000 + FDateTime::Now().GetMonth() * 100 + FDateTime::Now().GetDay();
		newDateObj->SetNumberField(TEXT("Date"), currentDate);
		TArray< TSharedPtr <FJsonValue>> notesArray;
		TSharedPtr<FJsonObject> newNoteObj = MakeShareable(new FJsonObject());
		newNoteObj->SetStringField(TEXT("1"), inNotes);
		notesArray.Add(MakeShareable(new FJsonValueObject(newNoteObj)));
		newDateObj->SetArrayField(TEXT("Notes"), notesArray);
		rootArray.Add(MakeShareable(new FJsonValueObject(newDateObj)));
	}
	//将rootArray转化为字符串,并保存到文件里
	FString outputString;
	TSharedRef<TJsonWriter<>> jsonWriter = TJsonWriterFactory<>::Create(&outputString);
	if (FJsonSerializer::Serialize(rootArray, jsonWriter)) {
		bool succus = FFileHelper::SaveStringToFile(outputString, *noteFilePath);
		return succus;
	}
	else UE_LOG(LogTemp, Warning, TEXT("---------->> 备注信息序列化失败!"));
	return false;
}
