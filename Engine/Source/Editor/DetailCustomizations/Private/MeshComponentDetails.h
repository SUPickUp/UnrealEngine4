// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FMeshComponentDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override;


private:
	IDetailCategoryBuilder* RenderingCategory;

};
