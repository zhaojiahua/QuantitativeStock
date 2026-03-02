// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RemarksInfoNotesWidget.generated.h"


UCLASS()
class QUANTITATIVESTOCK_API URemarksInfoNotesWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	//从指定路径的文本文件里加载备注信息
	UFUNCTION(BlueprintCallable, Category = "QT")
	bool GetNotesFromFile(FString& outNotes);
	//保存备注信息到指定路径的文本文件里
	UFUNCTION(BlueprintCallable, Category = "QT")
	bool SaveNotesToFile(const FString& inNotes);
	//当前股票的代码和名称，用于生成备注信息的文件名
	UPROPERTY(BlueprintReadWrite, Category = "QT | Params")
	FString currentCodeName{};

};
