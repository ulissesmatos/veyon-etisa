/*
 * AppBlockingFeaturePlugin.h - declaration of AppBlockingFeaturePlugin class
 *
 * Copyright (c) 2026 Veyon ETISA contributors
 *
 * This file is part of Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#pragma once

#include <QStringList>

#include "Feature.h"
#include "FeatureProviderInterface.h"

class QTimer;

class AppBlockingFeaturePlugin : public QObject, FeatureProviderInterface, PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.AppBlocking")
	Q_INTERFACES(PluginInterface FeatureProviderInterface)
public:
	enum class Argument
	{
		BlockedProcesses,
		BlockedDomains
	};
	Q_ENUM(Argument)

	explicit AppBlockingFeaturePlugin( QObject* parent = nullptr );
	~AppBlockingFeaturePlugin() override = default;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("5bbf313d-eb4e-44db-a328-3dbe803275c1") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral("AppBlocking");
	}

	QString description() const override
	{
		return tr( "Block applications and websites on a computer" );
	}

	QString vendor() const override
	{
		return QStringLiteral("Veyon ETISA");
	}

	QString copyright() const override
	{
		return QStringLiteral("Veyon ETISA contributors");
	}

	const FeatureList& featureList() const override
	{
		return m_features;
	}

	bool controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
						const ComputerControlInterfaceList& computerControlInterfaces ) override;

	bool startFeature( VeyonMasterInterface& master, const Feature& feature,
					   const ComputerControlInterfaceList& computerControlInterfaces ) override;

	bool handleFeatureMessage( VeyonServerInterface& server,
							   const MessageContext& messageContext,
							   const FeatureMessage& message ) override;

	bool isFeatureActive( VeyonServerInterface& server, Feature::Uid featureUid ) const override;

private:
	enum class FeatureCommand
	{
		StartBlocking,
		StopBlocking
	};

	static void applyDomainBlocklist( const QStringList& domains );
	static void removeDomainBlocklist();

	void enforceProcessBlocklist();

	const Feature m_appBlockingFeature;
	const FeatureList m_features;

	bool m_active;
	QStringList m_blockedProcesses;
	QStringList m_blockedDomains;
	QTimer* m_enforcementTimer;

};
