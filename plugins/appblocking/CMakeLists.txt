include(BuildVeyonPlugin)

build_veyon_plugin(appblocking
	NAME AppBlocking
	SOURCES
	AppBlockingFeaturePlugin.cpp
	BlockListDialog.cpp
	AppBlockingFeaturePlugin.h
	BlockListDialog.h
	BlockListDialog.ui
	)
