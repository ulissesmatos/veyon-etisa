/*
 * BlockListDialog.cpp - implementation of block list input dialog class
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

#include <QIcon>

#include "BlockListDialog.h"

#include "ui_BlockListDialog.h"


BlockListDialog::BlockListDialog( QStringList& blockedProcesses, QStringList& blockedDomains, QWidget* parent ) :
	QDialog( parent ),
	ui( new Ui::BlockListDialog ),
	m_blockedProcesses( blockedProcesses ),
	m_blockedDomains( blockedDomains )
{
	ui->setupUi( this );

	ui->processListEdit->setPlainText( blockedProcesses.join( QLatin1Char('\n') ) );
	ui->domainListEdit->setPlainText( blockedDomains.join( QLatin1Char('\n') ) );

	setWindowIcon( QIcon( QStringLiteral(":/core/media-playback-stop.png") ) );
}



BlockListDialog::~BlockListDialog()
{
	delete ui;
}



void BlockListDialog::accept()
{
	m_blockedProcesses = parseLines( ui->processListEdit->toPlainText() );
	m_blockedDomains = parseLines( ui->domainListEdit->toPlainText() );

	QDialog::accept();
}



QStringList BlockListDialog::parseLines( const QString& text )
{
	QStringList result;

	const auto lines = text.split( QLatin1Char('\n') );
	for( const auto& line : lines )
	{
		const auto trimmedLine = line.trimmed();
		if( trimmedLine.isEmpty() == false )
		{
			result << trimmedLine;
		}
	}

	return result;
}
