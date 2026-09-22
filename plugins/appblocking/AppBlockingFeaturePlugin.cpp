/*
 * AppBlockingFeaturePlugin.cpp - implementation of AppBlockingFeaturePlugin class
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

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#else
#include <csignal>
#include <unistd.h>
#endif

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTimer>

#include "AppBlockingFeaturePlugin.h"
#include "BlockListDialog.h"
#include "ComputerControlInterface.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"

namespace
{

// interval at which the blocked application list is re-checked so that
// applications started after blocking began are terminated as well
constexpr int EnforcementIntervalMs = 2000;


QString hostsFileMarker()
{
	return QStringLiteral("# veyon-appblocking");
}


QString hostsFilePath()
{
#ifdef Q_OS_WIN
	return qEnvironmentVariable( "SystemRoot", QStringLiteral("C:/Windows") ) +
			QStringLiteral("/System32/drivers/etc/hosts");
#else
	return QStringLiteral("/etc/hosts");
#endif
}


QStringList readHostsFileLines()
{
	QFile file( hostsFilePath() );
	if( file.open( QIODevice::ReadOnly | QIODevice::Text ) == false )
	{
		vWarning() << "AppBlocking: could not open hosts file for reading:" << file.fileName();
		return {};
	}

	return QString::fromUtf8( file.readAll() ).split( QLatin1Char('\n') );
}


bool writeHostsFileLines( const QStringList& lines )
{
	QFile file( hostsFilePath() );
	if( file.open( QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text ) == false )
	{
		vWarning() << "AppBlocking: could not open hosts file for writing:" << file.fileName();
		return false;
	}

	file.write( lines.join( QLatin1Char('\n') ).toUtf8() );

	return true;
}


void flushDnsCacheIfPossible()
{
#ifdef Q_OS_WIN
	QProcess::startDetached( QStringLiteral("ipconfig"), { QStringLiteral("/flushdns") } );
#endif
}


#ifdef Q_OS_WIN
void terminateBlockedProcesses( const QStringList& blockedProcessNames )
{
	const auto snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
	if( snapshot == INVALID_HANDLE_VALUE )
	{
		return;
	}

	PROCESSENTRY32W entry;
	entry.dwSize = sizeof(entry);

	if( Process32FirstW( snapshot, &entry ) )
	{
		do
		{
			const auto processName = QString::fromWCharArray( entry.szExeFile );

			for( const auto& blockedName : blockedProcessNames )
			{
				if( processName.compare( blockedName, Qt::CaseInsensitive ) == 0 )
				{
					const auto process = OpenProcess( PROCESS_TERMINATE, FALSE, entry.th32ProcessID );
					if( process != nullptr )
					{
						vDebug() << "AppBlocking: terminating" << processName << "with PID" << entry.th32ProcessID;
						TerminateProcess( process, 1 );
						CloseHandle( process );
					}
					break;
				}
			}
		}
		while( Process32NextW( snapshot, &entry ) );
	}

	CloseHandle( snapshot );
}
#else
void terminateBlockedProcesses( const QStringList& blockedProcessNames )
{
	QDir procDir( QStringLiteral("/proc") );
	const auto entries = procDir.entryList( QDir::Dirs | QDir::NoDotAndDotDot );

	for( const auto& entry : entries )
	{
		bool isPid = false;
		const auto pid = entry.toLong( &isPid );
		if( isPid == false )
		{
			continue;
		}

		QFile commFile( QStringLiteral("/proc/%1/comm").arg( entry ) );
		if( commFile.open( QIODevice::ReadOnly | QIODevice::Text ) == false )
		{
			continue;
		}

		const auto processName = QString::fromUtf8( commFile.readAll() ).trimmed();

		for( const auto& blockedName : blockedProcessNames )
		{
			if( processName.compare( blockedName, Qt::CaseInsensitive ) == 0 )
			{
				vDebug() << "AppBlocking: terminating" << processName << "with PID" << pid;
				::kill( static_cast<pid_t>( pid ), SIGTERM );
				break;
			}
		}
	}
}
#endif

} // namespace


AppBlockingFeaturePlugin::AppBlockingFeaturePlugin( QObject* parent ) :
	QObject( parent ),
	m_appBlockingFeature( QStringLiteral( "ApplicationWebsiteBlocking" ),
						  Feature::Flag::Mode | Feature::Flag::AllComponents,
						  Feature::Uid( "643ac3f8-2e1f-4850-88f3-d38477a2d310" ),
						  Feature::Uid(),
						  tr( "Block apps/websites" ), tr( "Unblock" ),
						  tr( "Block selected applications and websites on the computers "
							  "to keep students focused on the current task." ),
						  QStringLiteral(":/core/media-playback-stop.png") ),
	m_features( { m_appBlockingFeature } ),
	m_active( false ),
	m_enforcementTimer( nullptr )
{
}



bool AppBlockingFeaturePlugin::controlFeature( Feature::Uid featureUid,
											   Operation operation,
											   const QVariantMap& arguments,
											   const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( hasFeature( featureUid ) == false )
	{
		return false;
	}

	if( operation == Operation::Start )
	{
		const auto blockedProcesses = arguments.value( argToString(Argument::BlockedProcesses) ).toStringList();
		const auto blockedDomains = arguments.value( argToString(Argument::BlockedDomains) ).toStringList();

		sendFeatureMessage(FeatureMessage{featureUid, FeatureCommand::StartBlocking}
						   .addArgument(Argument::BlockedProcesses, blockedProcesses)
						   .addArgument(Argument::BlockedDomains, blockedDomains),
						   computerControlInterfaces);

		return true;
	}

	if( operation == Operation::Stop )
	{
		sendFeatureMessage(FeatureMessage{featureUid, FeatureCommand::StopBlocking}, computerControlInterfaces);

		return true;
	}

	return false;
}



bool AppBlockingFeaturePlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
											 const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( feature.uid() != m_appBlockingFeature.uid() )
	{
		return false;
	}

	QStringList blockedProcesses;
	QStringList blockedDomains;

	if( BlockListDialog( blockedProcesses, blockedDomains, master.mainWindow() ).exec() != QDialog::Accepted )
	{
		return true;
	}

	if( blockedProcesses.isEmpty() && blockedDomains.isEmpty() )
	{
		return true;
	}

	return controlFeature( m_appBlockingFeature.uid(), Operation::Start,
						   {
							   { argToString(Argument::BlockedProcesses), blockedProcesses },
							   { argToString(Argument::BlockedDomains), blockedDomains }
						   },
						   computerControlInterfaces );
}



bool AppBlockingFeaturePlugin::handleFeatureMessage( VeyonServerInterface& server,
													 const MessageContext& messageContext,
													 const FeatureMessage& message )
{
	Q_UNUSED(server)
	Q_UNUSED(messageContext)

	if( message.featureUid() != m_appBlockingFeature.uid() )
	{
		return false;
	}

	switch( message.command<FeatureCommand>() )
	{
	case FeatureCommand::StartBlocking:
		m_blockedProcesses = message.argument( Argument::BlockedProcesses ).toStringList();
		m_blockedDomains = message.argument( Argument::BlockedDomains ).toStringList();
		m_active = true;

		applyDomainBlocklist( m_blockedDomains );

		if( m_enforcementTimer == nullptr )
		{
			m_enforcementTimer = new QTimer( this );
			connect( m_enforcementTimer, &QTimer::timeout, this, &AppBlockingFeaturePlugin::enforceProcessBlocklist );
		}

		enforceProcessBlocklist();
		m_enforcementTimer->start( EnforcementIntervalMs );

		return true;

	case FeatureCommand::StopBlocking:
		m_active = false;
		m_blockedProcesses.clear();
		m_blockedDomains.clear();

		if( m_enforcementTimer != nullptr )
		{
			m_enforcementTimer->stop();
		}

		removeDomainBlocklist();

		return true;

	default:
		break;
	}

	return false;
}



bool AppBlockingFeaturePlugin::isFeatureActive( VeyonServerInterface& server, Feature::Uid featureUid ) const
{
	Q_UNUSED(server)

	return featureUid == m_appBlockingFeature.uid() && m_active;
}



void AppBlockingFeaturePlugin::applyDomainBlocklist( const QStringList& domains )
{
	auto lines = readHostsFileLines();

	lines.erase( std::remove_if( lines.begin(), lines.end(),
								 []( const QString& line ) { return line.contains( hostsFileMarker() ); } ),
				lines.end() );

	for( const auto& domain : domains )
	{
		const auto trimmedDomain = domain.trimmed();
		if( trimmedDomain.isEmpty() )
		{
			continue;
		}

		QStringList domainVariants{ trimmedDomain };
		if( trimmedDomain.startsWith( QStringLiteral("www."), Qt::CaseInsensitive ) == false )
		{
			domainVariants << QStringLiteral("www.%1").arg( trimmedDomain );
		}

		for( const auto& domainVariant : std::as_const(domainVariants) )
		{
			lines << QStringLiteral("127.0.0.1 %1 %2").arg( domainVariant, hostsFileMarker() );
			lines << QStringLiteral("::1 %1 %2").arg( domainVariant, hostsFileMarker() );
		}
	}

	if( writeHostsFileLines( lines ) )
	{
		flushDnsCacheIfPossible();
	}
}



void AppBlockingFeaturePlugin::removeDomainBlocklist()
{
	auto lines = readHostsFileLines();

	const auto lineCountBefore = lines.size();

	lines.erase( std::remove_if( lines.begin(), lines.end(),
								 []( const QString& line ) { return line.contains( hostsFileMarker() ); } ),
				lines.end() );

	if( lines.size() != lineCountBefore && writeHostsFileLines( lines ) )
	{
		flushDnsCacheIfPossible();
	}
}



void AppBlockingFeaturePlugin::enforceProcessBlocklist()
{
	if( m_blockedProcesses.isEmpty() == false )
	{
		terminateBlockedProcesses( m_blockedProcesses );
	}
}
