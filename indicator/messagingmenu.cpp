/*
 * Copyright (C) 2012 Canonical, Ltd.
 *
 * Authors:
 *  Gustavo Pichorim Boiko <gustavo.boiko@canonical.com>
 *
 * This file is part of telephony-service.
 *
 * telephony-service is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * telephony-service is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "applicationutils.h"
#include "config.h"
#include "contactutils.h"
#include "phoneutils.h"
#include "messagingmenu.h"
#include <QContactAvatar>
#include <QContactFetchRequest>
#include <QContactPhoneNumber>
#include <QDateTime>
#include <QDebug>
#include <gio/gio.h>
#include <messaging-menu-message.h>

namespace C {
#include <libintl.h>
}

#define SOURCE_ID "telephony-service-indicator"

QTCONTACTS_USE_NAMESPACE

MessagingMenu::MessagingMenu(QObject *parent) :
    QObject(parent), mVoicemailCount(-1)
{
    GIcon *icon = g_icon_new_for_string("telephony-service-indicator", NULL);
    mMessagesApp = messaging_menu_app_new("telephony-service-sms.desktop");
    messaging_menu_app_register(mMessagesApp);
    messaging_menu_app_append_source(mMessagesApp, SOURCE_ID, icon, C::gettext("Telephony Service"));

    mCallsApp = messaging_menu_app_new("telephony-service-call.desktop");
    messaging_menu_app_register(mCallsApp);
    messaging_menu_app_append_source(mCallsApp, SOURCE_ID, icon, C::gettext("Telephony Service"));

    g_object_unref(icon);
}

void MessagingMenu::addMessage(const QString &phoneNumber, const QString &messageId, const QDateTime &timestamp, const QString &text)
{
    // try to get a contact for that phone number
    QUrl iconPath = QUrl::fromLocalFile(telephonyServiceDir() + "/assets/avatar-default@18.png");
    QString contactAlias = phoneNumber;

    // try to match the contact info
    QContactFetchRequest *request = new QContactFetchRequest(this);
    request->setFilter(QContactPhoneNumber::match(phoneNumber));

    // place the messaging-menu item only after the contact fetch request is finished, as we can´t simply update
    QObject::connect(request, &QContactAbstractRequest::stateChanged,
                     [request, phoneNumber, messageId, text, timestamp, iconPath, contactAlias, this]() {
        // only process the results after the finished state is reached
        if (request->state() != QContactAbstractRequest::FinishedState) {
            return;
        }

        QString displayLabel;
        QUrl avatar;

        if (request->contacts().size() > 0) {
            QContact contact = request->contacts().at(0);
            displayLabel = ContactUtils::formatContactName(contact);
            avatar = contact.detail<QContactAvatar>().imageUrl();
        }

        if (displayLabel.isEmpty()) {
            displayLabel = contactAlias;
        }

        if (avatar.isEmpty()) {
            avatar = iconPath;
        }

        GFile *file = g_file_new_for_uri(avatar.toString().toUtf8().data());
        GIcon *icon = g_file_icon_new(file);
        MessagingMenuMessage *message = messaging_menu_message_new(messageId.toUtf8().data(),
                                                                   icon,
                                                                   displayLabel.toUtf8().data(),
                                                                   NULL,
                                                                   text.toUtf8().data(),
                                                                   timestamp.toMSecsSinceEpoch() * 1000); // the value is expected to be in microseconds
        messaging_menu_message_add_action(message,
                                          "quickReply",
                                          NULL, // label
                                          G_VARIANT_TYPE("s"),
                                          NULL // predefined values
                                          );
        g_signal_connect(message, "activate", G_CALLBACK(&MessagingMenu::messageActivateCallback), this);

        // save the phone number to use in the actions
        mMessages[messageId] = phoneNumber;
        messaging_menu_app_append_message(mMessagesApp, message, SOURCE_ID, true);

        g_object_unref(file);
        g_object_unref(icon);
        g_object_unref(message);
    });

    request->setManager(ContactUtils::sharedManager());
    request->start();
}

void MessagingMenu::removeMessage(const QString &messageId)
{
    if (!mMessages.contains(messageId)) {
        return;
    }

    messaging_menu_app_remove_message_by_id(mMessagesApp, messageId.toUtf8().data());
    mMessages.remove(messageId);
}

void MessagingMenu::addCall(const QString &phoneNumber, const QDateTime &timestamp)
{
    Call call;
    bool found = false;
    Q_FOREACH(Call callMessage, mCalls) {
        if (PhoneUtils::isSameContact(callMessage.number, phoneNumber)) {
            call = callMessage;
            found = true;
            mCalls.removeOne(callMessage);

            // remove the previous entry and add a new one increasing the missed call count
            messaging_menu_app_remove_message_by_id(mCallsApp, callMessage.messageId.toUtf8().data());
            break;
        }
    }

    if (!found) {
        call.contactAlias = phoneNumber;
        call.contactIcon = QUrl::fromLocalFile(telephonyServiceDir() + "/assets/avatar-default@18.png");
        call.number = phoneNumber;
        call.count = 0;
    }

    call.count++;

    QString text;
    text = QString::fromUtf8(C::ngettext("%1 missed call", "%1 missed calls", call.count)).arg(call.count);

    // try to match the contact info
    QContactFetchRequest *request = new QContactFetchRequest(this);
    request->setFilter(QContactPhoneNumber::match(phoneNumber));

    // place the messaging-menu item only after the contact fetch request is finished, as we can´t simply update
    QObject::connect(request, &QContactAbstractRequest::stateChanged, [request, call, text, timestamp, this]() {
        // only process the results after the finished state is reached
        if (request->state() != QContactAbstractRequest::FinishedState) {
            return;
        }

        Call newCall = call;
        if (request->contacts().size() > 0) {
            QContact contact = request->contacts().at(0);
            QString displayLabel = ContactUtils::formatContactName(contact);
            QUrl avatar = contact.detail<QContactAvatar>().imageUrl();

            if (!displayLabel.isEmpty()) {
                newCall.contactAlias = displayLabel;
            }

            if (!avatar.isEmpty()) {
                newCall.contactIcon = avatar;
            }
        }

        GFile *file = g_file_new_for_uri(newCall.contactIcon.toString().toUtf8().data());
        GIcon *icon = g_file_icon_new(file);
        MessagingMenuMessage *message = messaging_menu_message_new(newCall.number.toUtf8().data(),
                                                                   icon,
                                                                   newCall.contactAlias.toUtf8().data(),
                                                                   NULL,
                                                                   text.toUtf8().data(),
                                                                   timestamp.toMSecsSinceEpoch() * 1000);  // the value is expected to be in microseconds
        newCall.messageId = messaging_menu_message_get_id(message);
        messaging_menu_message_add_action(message,
                                          "callBack",
                                          NULL, // label
                                          NULL, // argument type
                                          NULL // predefined values
                                          );
        const char *predefinedMessages[] = {
                C::gettext("I missed your call - can you call me now?"),
                C::gettext("I'm running late. I'm on my way."),
                C::gettext("I'm busy at the moment. I'll call you later."),
                C::gettext("I'll be 20 minutes late."),
                C::gettext("Sorry, I'm still busy. I'll call you later."),
                0
                };
        GVariant *messages = g_variant_new_strv(predefinedMessages, -1);
        messaging_menu_message_add_action(message,
                                          "replyWithMessage",
                                          NULL, // label
                                          G_VARIANT_TYPE("s"),
                                          messages // predefined values
                                          );
        g_signal_connect(message, "activate", G_CALLBACK(&MessagingMenu::callsActivateCallback), this);
        messaging_menu_app_append_message(mCallsApp, message, SOURCE_ID, true);
        mCalls.append(newCall);

        g_object_unref(messages);
        g_object_unref(file);
        g_object_unref(icon);
        g_object_unref(message);
    });

    request->setManager(ContactUtils::sharedManager());
    request->start();
}

void MessagingMenu::showVoicemailEntry(int count)
{
    if (!mVoicemailId.isEmpty()) {
        // if the count didn't change, don't do anything
        if (count == mVoicemailCount) {
            return;
        }

        messaging_menu_app_remove_message_by_id(mCallsApp, mVoicemailId.toUtf8().data());
    }

    QString messageBody = C::gettext("Voicemail messages");
    if (count != 0) {
        messageBody = QString::fromUtf8(C::ngettext("%1 voicemail message", "%1 voicemail messages", count)).arg(count);
    }

    GIcon *icon = g_themed_icon_new("indicator-call");
    MessagingMenuMessage *message = messaging_menu_message_new("voicemail",
                                                               icon,
                                                               "Voicemail",
                                                               NULL,
                                                               messageBody.toUtf8().data(),
                                                               QDateTime::currentDateTime().toMSecsSinceEpoch() * 1000); // the value is expected to be in microseconds
    g_signal_connect(message, "activate", G_CALLBACK(&MessagingMenu::callsActivateCallback), this);
    mVoicemailId = "voicemail";

    g_object_unref(icon);
    g_object_unref(message);
}

void MessagingMenu::hideVoicemailEntry()
{
    if (!mVoicemailId.isEmpty()) {
        messaging_menu_app_remove_message_by_id(mCallsApp, mVoicemailId.toUtf8().data());
        mVoicemailId = "";
    }
}

void MessagingMenu::messageActivateCallback(MessagingMenuMessage *message, const char *actionId, GVariant *param, MessagingMenu *instance)
{
    QString action(actionId);
    QString messageId(messaging_menu_message_get_id(message));
    QString text(g_variant_get_string(param, NULL));

    if (action == "quickReply") {
        QMetaObject::invokeMethod(instance, "sendMessageReply", Q_ARG(QString, messageId), Q_ARG(QString, text));
    } else if (action.isEmpty()) {
        QMetaObject::invokeMethod(instance, "showMessage", Q_ARG(QString, messageId));
    }
}

void MessagingMenu::callsActivateCallback(MessagingMenuMessage *message, const char *actionId, GVariant *param, MessagingMenu *instance)
{
    QString action(actionId);
    QString messageId(messaging_menu_message_get_id(message));

    if (action == "callBack") {
        QMetaObject::invokeMethod(instance, "callBack", Q_ARG(QString, messageId));
    } else if (action == "replyWithMessage") {
        QString text(g_variant_get_string(param, NULL));
        QMetaObject::invokeMethod(instance, "replyWithMessage", Q_ARG(QString, messageId), Q_ARG(QString, text));
    } else if (messageId == instance->mVoicemailId) {
        QMetaObject::invokeMethod(instance, "callVoicemail", Q_ARG(QString, messageId));
    }
}

void MessagingMenu::sendMessageReply(const QString &messageId, const QString &reply)
{
    QString phoneNumber = mMessages[messageId];
    Q_EMIT replyReceived(phoneNumber, reply);

    Q_EMIT messageRead(phoneNumber, messageId);
}

void MessagingMenu::showMessage(const QString &messageId)
{
    QString phoneNumber = mMessages[messageId];
    ApplicationUtils::openUrl(QString("message:///%1").arg(QString(QUrl::toPercentEncoding(phoneNumber))));
}

void MessagingMenu::callBack(const QString &messageId)
{
    QString phoneNumber = callFromMessageId(messageId).number;
    qDebug() << "TelephonyService/MessagingMenu: Calling back" << phoneNumber;
    ApplicationUtils::openUrl(QString("tel:///%1").arg(QString(QUrl::toPercentEncoding(phoneNumber))));
}

void MessagingMenu::replyWithMessage(const QString &messageId, const QString &reply)
{
    QString phoneNumber = callFromMessageId(messageId).number;
    qDebug() << "TelephonyService/MessagingMenu: Replying to call" << phoneNumber << "with text" << reply;
    Q_EMIT replyReceived(phoneNumber, reply);
}

void MessagingMenu::callVoicemail(const QString &messageId)
{
    qDebug() << "TelephonyService/MessagingMenu: Calling voicemail for messageId" << messageId;
    ApplicationUtils::openUrl(QUrl("tel:///voicemail"));
}

Call MessagingMenu::callFromMessageId(const QString &messageId)
{
    Q_FOREACH(const Call &call, mCalls) {
        if (call.messageId == messageId) {
            return call;
        }
     }
    return Call();
}


MessagingMenu *MessagingMenu::instance()
{
    static MessagingMenu *menu = new MessagingMenu();
    return menu;
}

MessagingMenu::~MessagingMenu()
{
    g_object_unref(mMessagesApp);
    g_object_unref(mCallsApp);
}
