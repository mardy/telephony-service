/*
 * Copyright (C) 2013 Canonical, Ltd.
 *
 * Authors:
 *    Gustavo Pichorim Boiko <gustavo.boiko@canonical.com>
 *
 * This file is part of phone-app.
 *
 * phone-app is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * phone-app is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "callhandler.h"
#include "phoneapphandler.h"
#include "phoneapphandlerdbus.h"
#include "telepathyhelper.h"
#include "texthandler.h"
#include <QCoreApplication>
#include <TelepathyQt/ClientRegistrar>
#include <TelepathyQt/AbstractClient>
#include <TelepathyQt/AccountManager>
#include <TelepathyQt/Contact>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    Tp::registerTypes();

    PhoneAppHandler *handler = new PhoneAppHandler();
    TelepathyHelper::instance()->registerClient(handler, "PhoneAppHandler");

    QObject::connect(handler, SIGNAL(callChannelAvailable(Tp::CallChannelPtr)),
                     CallHandler::instance(), SLOT(onCallChannelAvailable(Tp::CallChannelPtr)));
    QObject::connect(handler, SIGNAL(textChannelAvailable(Tp::TextChannelPtr)),
                     TextHandler::instance(), SLOT(onTextChannelAvailable(Tp::TextChannelPtr)));

    PhoneAppHandlerDBus dbus;

    QObject::connect(TelepathyHelper::instance(), SIGNAL(accountReady()),
                     &dbus, SLOT(connectToBus()));

    return app.exec();
}