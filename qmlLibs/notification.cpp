/*
 * Copyright (C) 2013 Jolla Ltd.
 * Contact: Vesa Halttunen <vesa.halttunen@jollamobile.com>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */
#include "notificationmanagerproxy.h"
#include "notification.h"

#include <QStringBuilder>

namespace {

const char *HINT_CATEGORY = "category";
const char *HINT_URGENCY = "urgency";
const char *HINT_ITEM_COUNT = "x-nemo-item-count";
const char *HINT_TIMESTAMP = "x-nemo-timestamp";
const char *HINT_PREVIEW_BODY = "x-nemo-preview-body";
const char *HINT_PREVIEW_SUMMARY = "x-nemo-preview-summary";
const char *HINT_REMOTE_ACTION_PREFIX = "x-nemo-remote-action-";
const char *HINT_REMOTE_ACTION_ICON_PREFIX = "x-nemo-remote-action-icon-";
const char *HINT_ORIGIN = "x-nemo-origin";
const char *HINT_OWNER = "x-nemo-owner";
const char *HINT_MAX_CONTENT_LINES = "x-nemo-max-content-lines";
const char *DEFAULT_ACTION_NAME = "default";

static inline QString processName() {
    // Defaults to the filename if not set
    return QCoreApplication::applicationName();
}

//! A proxy for accessing the notification manager
Q_GLOBAL_STATIC_WITH_ARGS(NotificationManagerProxy, notificationManagerProxyInstance, ("org.freedesktop.Notifications", "/org/freedesktop/Notifications", QDBusConnection::sessionBus()))

NotificationManagerProxy *notificationManager()
{
    if (!notificationManagerProxyInstance.exists()) {
        qDBusRegisterMetaType<NotificationData>();
        qDBusRegisterMetaType<QList<NotificationData> >();
    }

    return notificationManagerProxyInstance();
}

QString encodeDBusCall(const QString &service, const QString &path, const QString &iface, const QString &method, const QVariantList &arguments)
{
    const QString space(QStringLiteral(" "));

    QString s = service % space % path % space % iface % space % method;

    if (!arguments.isEmpty()) {
        QStringList args;
        int argsLength = 0;

        foreach (const QVariant &arg, arguments) {
            // Serialize the QVariant into a Base64 encoded byte stream
            QByteArray buffer;
            QDataStream stream(&buffer, QIODevice::WriteOnly);
            stream << arg;
            args.append(space + buffer.toBase64());
            argsLength += args.last().length();
        }

        s.reserve(s.length() + argsLength);
        foreach (const QString &arg, args) {
            s.append(arg);
        }
    }

    return s;
}

QStringList encodeActions(const QHash<QString, QString> &actions)
{
    QStringList rv;

    // Actions are encoded as a sequence of name followed by displayName
    QHash<QString, QString>::const_iterator it = actions.constBegin(), end = actions.constEnd();
    for ( ; it != end; ++it) {
        rv.append(it.key());
        rv.append(it.value());
    }

    return rv;
}

QHash<QString, QString> decodeActions(const QStringList &actions)
{
    QHash<QString, QString> rv;

    QStringList::const_iterator it = actions.constBegin(), end = actions.constEnd();
    while (it != end) {
        // If we have an odd number of tokens, add an empty displayName to complete the last pair
        const QString &name(*it);
        QString displayName;
        if (++it != end) {
            displayName = *it;
            ++it;
        }
        rv.insert(name, displayName);
    }

    return rv;
}

QPair<QHash<QString, QString>, QVariantHash> encodeActionHints(const QVariantList &actions)
{
    QPair<QHash<QString, QString>, QVariantHash> rv;

    foreach (const QVariant &action, actions) {
        QVariantMap vm = action.value<QVariantMap>();
        const QString actionName = vm["name"].value<QString>();
        if (!actionName.isEmpty()) {
            const QString displayName = vm["displayName"].value<QString>();
            const QString service = vm["service"].value<QString>();
            const QString path = vm["path"].value<QString>();
            const QString iface = vm["iface"].value<QString>();
            const QString method = vm["method"].value<QString>();
            const QVariantList arguments = vm["arguments"].value<QVariantList>();
            const QString icon = vm["icon"].value<QString>();

            if (!service.isEmpty() && !path.isEmpty() && !iface.isEmpty() && !method.isEmpty()) {
                rv.first.insert(actionName, displayName);
                rv.second.insert(QString(HINT_REMOTE_ACTION_PREFIX) + actionName, encodeDBusCall(service, path, iface, method, arguments));
                if (!icon.isEmpty()) {
                    rv.second.insert(QString(HINT_REMOTE_ACTION_ICON_PREFIX) + actionName, icon);
                }
            }
        }
    }

    return rv;
}

QVariantList decodeActionHints(const QHash<QString, QString> &actions, const QVariantHash &hints)
{
    QVariantList rv;

    QHash<QString, QString>::const_iterator ait = actions.constBegin(), aend = actions.constEnd();
    for ( ; ait != aend; ++ait) {
        const QString &actionName = ait.key();
        const QString &displayName = ait.value();

        const QString hintName = QString(HINT_REMOTE_ACTION_PREFIX) + actionName;
        const QString &hint = hints[hintName].toString();
        if (!hint.isEmpty()) {
            QVariantMap action;

            // Extract the element of the DBus call
            QStringList elements(hint.split(' ', QString::SkipEmptyParts));
            if (elements.size() <= 3) {
                qWarning() << "Unable to decode invalid remote action:" << hint;
            } else {
                int index = 0;
                action.insert(QStringLiteral("service"), elements.at(index++));
                action.insert(QStringLiteral("path"), elements.at(index++));
                action.insert(QStringLiteral("iface"), elements.at(index++));
                action.insert(QStringLiteral("method"), elements.at(index++));

                QVariantList args;
                while (index < elements.size()) {
                    const QString &arg(elements.at(index++));
                    const QByteArray buffer(QByteArray::fromBase64(arg.toUtf8()));

                    QDataStream stream(buffer);
                    QVariant var;
                    stream >> var;
                    args.append(var);
                }
                action.insert(QStringLiteral("arguments"), args);

                action.insert(QStringLiteral("name"), actionName);
                action.insert(QStringLiteral("displayName"), displayName);

                const QString iconHintName = QString(HINT_REMOTE_ACTION_ICON_PREFIX) + actionName;
                const QString &iconHint = hints[iconHintName].toString();
                if (!iconHint.isEmpty()) {
                    action.insert(QStringLiteral("icon"), iconHint);
                }

                rv.append(action);
            }
        }
    }

    return rv;
}

}

NotificationData::NotificationData()
    : replacesId(0)
    , expireTimeout(-1)
{
}

class NotificationPrivate : public NotificationData
{
    friend class Notification;

    NotificationPrivate()
        : NotificationData()
    {
    }

    NotificationPrivate(const NotificationData &data)
        : NotificationData(data)
        , remoteActions(decodeActionHints(actions, hints))
    {
    }

    QVariantMap firstRemoteAction() const
    {
        QVariantMap vm;
        const QVariant firstAction(remoteActions.value(0));
        if (!firstAction.isNull()) {
            vm = firstAction.value<QVariantMap>();
        }
        return vm;
    }

    void setFirstRemoteAction(QVariantMap vm, Notification *q)
    {
        QString name(vm["name"].value<QString>());
        if (name.isEmpty()) {
            vm.insert("name", QString::fromLatin1(DEFAULT_ACTION_NAME));
        }
        q->setRemoteActions(QVariantList() << vm);
    }

    QVariantList remoteActions;
};

/*!
    \qmltype Notification
    \brief Allows notifications to be published

    The Notification class is a convenience class for using notifications
    based on the
    \l {http://www.galago-project.org/specs/notification/0.9/} {Desktop
    Notifications Specification} as implemented in Nemo.

    Since the Nemo implementation allows static notification parameters,
    such as icon, urgency, priority and user closability definitions, to
    be defined as part of notification category definitions, this
    convenience class is kept as simple as possible by allowing only the
    dynamic parameters, such as summary and body text, item count and
    timestamp to be defined. Other parameters should be defined in the
    notification category definition. Please refer to Lipstick documentation
    for a complete description of the category definition files.

    An example of the usage of this class from a Qml application:

    \qml
    Button {
        Notification {
            id: notification
            category: "x-nemo.example"
            appName: "Example App"
            appIcon: "/usr/share/example-app/icon-l-application"
            summary: "Notification summary"
            body: "Notification body"
            previewSummary: "Notification preview summary"
            previewBody: "Notification preview body"
            itemCount: 5
            timestamp: "2013-02-20 18:21:00"
            remoteActions: [ {
                "name": "default",
                "displayName": "Do something",
                "icon": "icon-s-do-it",
                "service": "org.nemomobile.example",
                "path": "/example",
                "iface": "org.nemomobile.example",
                "method": "doSomething",
                "arguments": [ "argument", 1 ]
            },{
                "name": "ignore",
                "displayName": "Ignore the problem",
                "icon": "icon-s-ignore",
                "service": "org.nemomobile.example",
                "path": "/example",
                "iface": "org.nemomobile.example",
                "method": "ignore",
                "arguments": [ "argument", 1 ]
            } ]
            onClicked: console.log("Clicked")
            onClosed: console.log("Closed, reason: " + reason)
        }
        text: "Application notification, ID " + notification.replacesId
        onClicked: notification.publish()
    }
    \endqml

    An example category definition file
    /usr/share/lipstick/notificationcategories/x-nemo.example.conf:

    \code
    x-nemo-icon=icon-lock-sms
    x-nemo-preview-icon=icon-s-status-sms
    x-nemo-feedback=sms
    x-nemo-priority=70
    x-nemo-user-removable=true
    x-nemo-user-closeable=false
    \endcode

    After publishing the ID of the notification can be found from the
    replacesId property.

    Notification::notifications() can be used to fetch notifications
    created by the calling application.
 */
Notification::Notification(QObject *parent) :
    QObject(parent),
    d_ptr(new NotificationPrivate)
{
    connect(notificationManager(), SIGNAL(ActionInvoked(uint,QString)), this, SLOT(checkActionInvoked(uint,QString)));
    connect(notificationManager(), SIGNAL(NotificationClosed(uint,uint)), this, SLOT(checkNotificationClosed(uint,uint)));
}

Notification::Notification(const NotificationData &data, QObject *parent) :
    QObject(parent),
    d_ptr(new NotificationPrivate(data))
{
    connect(notificationManager(), SIGNAL(ActionInvoked(uint,QString)), this, SLOT(checkActionInvoked(uint,QString)));
    connect(notificationManager(), SIGNAL(NotificationClosed(uint,uint)), this, SLOT(checkNotificationClosed(uint,uint)));
}

Notification::~Notification()
{
    delete d_ptr;
}

/*!
    \qmlproperty QString Notification::category

    The type of notification this is. Defaults to an empty string.
 */
QString Notification::category() const
{
    Q_D(const Notification);
    return d->hints.value(HINT_CATEGORY).toString();
}

void Notification::setCategory(const QString &category)
{
    Q_D(Notification);
    if (category != this->category()) {
        d->hints.insert(HINT_CATEGORY, category);
        emit categoryChanged();
    }
}

/*!
    \qmlproperty QString Notification::appName

    The application name associated with this notification, for display purposes.

    The application name should be the formal name, localized if appropriate.
 */
QString Notification::appName() const
{
    Q_D(const Notification);
    return d->appName;
}

void Notification::setAppName(const QString &appName)
{
    Q_D(Notification);
    if (appName != this->appName()) {
        d->appName = appName;
        emit appNameChanged();
    }
}

/*!
    \qmlproperty uint Notification::replacesId

    The optional notification ID that this notification replaces. The server must atomically (ie with no flicker or other visual cues) replace the given notification with this one. This allows clients to effectively modify the notification while it's active. A value of value of 0 means that this notification won't replace any existing notifications. Defaults to 0.
 */
uint Notification::replacesId() const
{
    Q_D(const Notification);
    return d->replacesId;
}

void Notification::setReplacesId(uint id)
{
    Q_D(Notification);
    if (d->replacesId != id) {
        d->replacesId = id;
        emit replacesIdChanged();
    }
}

/*!
    \qmlproperty QString Notification::appIcon

    The icon associated with this notification's application. Defaults to empty.
 */
QString Notification::appIcon() const
{
    Q_D(const Notification);
    return d->appIcon;
}

void Notification::setAppIcon(const QString &appIcon)
{
    Q_D(Notification);
    if (appIcon != this->appIcon()) {
        d->appIcon = appIcon;
        emit appIconChanged();
    }
}

/*!
    \qmlproperty QString Notification::summary

    The summary text briefly describing the notification. Defaults to an empty string.
 */
QString Notification::summary() const
{
    Q_D(const Notification);
    return d->summary;
}

void Notification::setSummary(const QString &summary)
{
    Q_D(Notification);
    if (d->summary != summary) {
        d->summary = summary;
        emit summaryChanged();
    }
}

/*!
    \qmlproperty QString Notification::body

    The optional detailed body text. Can be empty. Defaults to an empty string.
 */
QString Notification::body() const
{
    Q_D(const Notification);
    return d->body;
}

void Notification::setBody(const QString &body)
{
    Q_D(Notification);
    if (d->body != body) {
        d->body = body;
        emit bodyChanged();
    }
}

/*!
    \qmlproperty enumeration Notification::urgency

    The urgency level of the notification.

    Defaults to Normal urgency.
 */
Notification::Urgency Notification::urgency() const
{
    Q_D(const Notification);
    // Clipping to bounds in case an invalid value is stored as a hint
    return static_cast<Urgency>(qMax(static_cast<int>(Low), qMin(static_cast<int>(Critical), d->hints.value(HINT_URGENCY).toInt())));
}

void Notification::setUrgency(Urgency urgency)
{
    Q_D(Notification);
    if (urgency != this->urgency()) {
        d->hints.insert(HINT_URGENCY, static_cast<int>(urgency));
        emit urgencyChanged();
    }
}

/*!
    \qmlproperty int32 Notification::expireTimeout

    The number of milliseconds after display at which the notification should be automatically closed.
    ExpireTimeout of zero indicates that the notification should not close automatically.

    Defaults to -1, indicating that the notification manager should decide the expiration timeout.
 */
qint32 Notification::expireTimeout() const
{
    Q_D(const Notification);
    return d->expireTimeout;
}

void Notification::setExpireTimeout(qint32 milliseconds)
{
    Q_D(Notification);
    if (milliseconds != d->expireTimeout) {
        d->expireTimeout = milliseconds;
        emit expireTimeoutChanged();
    }
}

/*!
    \qmlproperty QDateTime Notification::timestamp

    The timestamp for the notification. Should be set to the time when the event the notification is related to has occurred. Defaults to current time.
 */
QDateTime Notification::timestamp() const
{
    Q_D(const Notification);
    return d->hints.value(HINT_TIMESTAMP).toDateTime();
}

void Notification::setTimestamp(const QDateTime &timestamp)
{
    Q_D(Notification);
    if (timestamp != this->timestamp()) {
        d->hints.insert(HINT_TIMESTAMP, timestamp.toString(Qt::ISODate));
        emit timestampChanged();
    }
}

/*!
    \qmlproperty QString Notification::previewSummary

    Summary text to be shown in the preview banner for the notification, if any. Defaults to an empty string.
 */
QString Notification::previewSummary() const
{
    Q_D(const Notification);
    return d->hints.value(HINT_PREVIEW_SUMMARY).toString();
}

void Notification::setPreviewSummary(const QString &previewSummary)
{
    Q_D(Notification);
    if (previewSummary != this->previewSummary()) {
        d->hints.insert(HINT_PREVIEW_SUMMARY, previewSummary);
        emit previewSummaryChanged();
    }
}

/*!
    \qmlproperty QString Notification::previewBody

    Body text to be shown in the preview banner for the notification, if any. Defaults to an empty string.
 */
QString Notification::previewBody() const
{
    Q_D(const Notification);
    return d->hints.value(HINT_PREVIEW_BODY).toString();
}

void Notification::setPreviewBody(const QString &previewBody)
{
    Q_D(Notification);
    if (previewBody != this->previewBody()) {
        d->hints.insert(HINT_PREVIEW_BODY, previewBody);
        emit previewBodyChanged();
    }
}

/*!
    \qmlproperty int Notification::itemCount

    The number of items represented by the notification. For example, a single notification can represent four missed calls by setting the count to 4. Defaults to 1.
 */
int Notification::itemCount() const
{
    Q_D(const Notification);
    return d->hints.value(HINT_ITEM_COUNT).toInt();
}

void Notification::setItemCount(int itemCount)
{
    Q_D(Notification);
    if (itemCount != this->itemCount()) {
        d->hints.insert(HINT_ITEM_COUNT, itemCount);
        emit itemCountChanged();
    }
}

/*!
    \qmlmethod void Notification::publish()

    Publishes the notification. If replacesId is 0, it will be a new
    notification. Otherwise the existing notification with the given ID
    is updated with the new details.
*/
void Notification::publish()
{
    Q_D(Notification);

    // Validate the actions associated with the notification
    Q_FOREACH (const QVariant &action, d->remoteActions) {
        const QVariantMap &vm = action.value<QVariantMap>();
        if (vm["name"].value<QString>().isEmpty()
                || vm["service"].value<QString>().isEmpty()
                || vm["path"].value<QString>().isEmpty()
                || vm["iface"].value<QString>().isEmpty()
                || vm["method"].value<QString>().isEmpty()) {
            qWarning() << "Invalid remote action specification:" << action;
        }
    }

    // Ensure the ownership of this notification is recorded
    QVariantHash::iterator it = d->hints.find(HINT_OWNER);
    if (it == d->hints.end()) {
        d->hints.insert(HINT_OWNER, processName());
    }

    setReplacesId(notificationManager()->Notify(appName(), d->replacesId, d->appIcon, d->summary, d->body,
                                                encodeActions(d->actions), d->hints, d->expireTimeout));
}

/*!
    \qmlmethod void Notification::close()

    Closes the notification if it has been published.
*/
void Notification::close()
{
    Q_D(Notification);
    if (d->replacesId != 0) {
        notificationManager()->CloseNotification(d->replacesId);
        setReplacesId(0);
    }
}

/*!
    \qmlsignal Notification::clicked()

    Sent when the notification is clicked (its default action is invoked).
*/
void Notification::checkActionInvoked(uint id, QString actionKey)
{
    Q_D(Notification);
    if (id == d->replacesId) {
        if (actionKey == DEFAULT_ACTION_NAME) {
            emit clicked();
        }
    }
}

/*!
    \qmlsignal Notification::closed(uint reason)

    Sent when the notification has been closed.
    \a reason is 1 if the notification expired, 2 if the notification was
    dismissed by the user, 3 if the notification was closed by a call to
    CloseNotification.
*/
void Notification::checkNotificationClosed(uint id, uint reason)
{
    Q_D(Notification);
    if (id == d->replacesId) {
        emit closed(reason);
        setReplacesId(0);
    }
}

/*!
    \qmlproperty QString Notification::remoteDBusCallServiceName

    The service name of the D-Bus call for this notification. Defaults to an empty string.
 */
QString Notification::remoteDBusCallServiceName() const
{
    Q_D(const Notification);
    QVariantMap vm(d->firstRemoteAction());
    return vm["service"].value<QString>();
}

void Notification::setRemoteDBusCallServiceName(const QString &serviceName)
{
    Q_D(Notification);
    QVariantMap vm(d->firstRemoteAction());
    if (vm["service"].value<QString>() != serviceName) {
        vm.insert("service", serviceName);
        d->setFirstRemoteAction(vm, this);

        emit remoteActionsChanged();
        emit remoteDBusCallChanged();
    }
}

/*!
    \qmlproperty QString Notification::remoteDBusCallObjectPath

    The object path of the D-Bus call for this notification. Defaults to an empty string.
 */
QString Notification::remoteDBusCallObjectPath() const
{
    Q_D(const Notification);
    QVariantMap vm(d->firstRemoteAction());
    return vm["path"].value<QString>();
}

void Notification::setRemoteDBusCallObjectPath(const QString &objectPath)
{
    Q_D(Notification);
    QVariantMap vm(d->firstRemoteAction());
    if (vm["path"].value<QString>() != objectPath) {
        vm.insert("path", objectPath);
        d->setFirstRemoteAction(vm, this);

        emit remoteActionsChanged();
        emit remoteDBusCallChanged();
    }
}

/*!
    \qmlproperty QString Notification::remoteDBusCallInterface

    The interface of the D-Bus call for this notification. Defaults to an empty string.
 */
QString Notification::remoteDBusCallInterface() const
{
    Q_D(const Notification);
    QVariantMap vm(d->firstRemoteAction());
    return vm["iface"].value<QString>();
}

void Notification::setRemoteDBusCallInterface(const QString &interface)
{
    Q_D(Notification);
    QVariantMap vm(d->firstRemoteAction());
    if (vm["iface"].value<QString>() != interface) {
        vm.insert("iface", interface);
        d->setFirstRemoteAction(vm, this);

        emit remoteActionsChanged();
        emit remoteDBusCallChanged();
    }
}

/*!
    \qmlproperty QString Notification::remoteDBusCallMethodName

    The method name of the D-Bus call for this notification. Defaults to an empty string.
 */
QString Notification::remoteDBusCallMethodName() const
{
    Q_D(const Notification);
    QVariantMap vm(d->firstRemoteAction());
    return vm["method"].value<QString>();
}

void Notification::setRemoteDBusCallMethodName(const QString &methodName)
{
    Q_D(Notification);
    QVariantMap vm(d->firstRemoteAction());
    if (vm["method"].value<QString>() != methodName) {
        vm.insert("method", methodName);
        d->setFirstRemoteAction(vm, this);

        emit remoteActionsChanged();
        emit remoteDBusCallChanged();
    }
}

/*!
    \qmlproperty QVariantList Notification::remoteDBusCallArguments

    The arguments of the D-Bus call for this notification. Defaults to an empty variant list.
 */
QVariantList Notification::remoteDBusCallArguments() const
{
    Q_D(const Notification);
    QVariantMap vm(d->firstRemoteAction());
    return vm["arguments"].value<QVariantList>();
}

void Notification::setRemoteDBusCallArguments(const QVariantList &arguments)
{
    Q_D(Notification);
    QVariantMap vm(d->firstRemoteAction());
    if (vm["arguments"].value<QVariantList>() != arguments) {
        vm.insert("arguments", arguments);
        d->setFirstRemoteAction(vm, this);

        emit remoteActionsChanged();
        emit remoteDBusCallChanged();
    }
}

/*!
    \qmlproperty QVariantList Notification::remoteActions

    The remote actions registered for potential invocation by this notification.
 */
QVariantList Notification::remoteActions() const
{
    Q_D(const Notification);
    return d->remoteActions;
}

void Notification::setRemoteActions(const QVariantList &remoteActions)
{
    Q_D(Notification);
    if (remoteActions != d->remoteActions) {
        // Remove any existing actions
        foreach (const QVariant &action, d->remoteActions) {
            QVariantMap vm = action.value<QVariantMap>();
            const QString actionName = vm["name"].value<QString>();
            if (!actionName.isEmpty()) {
                d->hints.remove(QString(HINT_REMOTE_ACTION_PREFIX) + actionName);
                d->actions.remove(actionName);
            }
        }

        // Add the new actions and their associated hints
        d->remoteActions = remoteActions;

        QPair<QHash<QString, QString>, QVariantHash> actionHints = encodeActionHints(remoteActions);

        QHash<QString, QString>::const_iterator ait = actionHints.first.constBegin(), aend = actionHints.first.constEnd();
        for ( ; ait != aend; ++ait) {
            d->actions.insert(ait.key(), ait.value());
        }

        QVariantHash::const_iterator hit = actionHints.second.constBegin(), hend = actionHints.second.constEnd();
        for ( ; hit != hend; ++hit) {
            d->hints.insert(hit.key(), hit.value());
        }

        emit remoteActionsChanged();
        emit remoteDBusCallChanged();
    }
}

/*!
    \qmlproperty QString Notification::origin

    A property indicating the origin of the notification.
    For example, the email address from which a mail was received that caused the notification to be created.
*/
QString Notification::origin() const
{
    Q_D(const Notification);
    return d->hints.value(HINT_ORIGIN).toString();
}

void Notification::setOrigin(const QString &origin)
{
    Q_D(Notification);
    if (origin != this->origin()) {
        d->hints.insert(HINT_ORIGIN, origin);
        emit originChanged();
    }
}

/*!
    \qmlproperty int Notification::maxContentLines

    A property suggesting the maximum amount of content to display for the notification.
*/
int Notification::maxContentLines() const
{
    Q_D(const Notification);
    return d->hints.value(HINT_MAX_CONTENT_LINES).toInt();
}

void Notification::setMaxContentLines(int max)
{
    Q_D(Notification);
    if (max != this->maxContentLines()) {
        d->hints.insert(HINT_MAX_CONTENT_LINES, max);
        emit maxContentLinesChanged();
    }
}

/*!
    \fn QVariant Notification::hintValue(const QString &hint) const

    Returns the value of the given \a hint .
*/
QVariant Notification::hintValue(const QString &hint) const
{
    Q_D(const Notification);
    return d->hints.value(hint);
}

/*!
    \fn void Notification::setHintValue(const QString &hint, const QVariant &value)

    Sets the value of the given \a hint to a given \a value .
*/
void Notification::setHintValue(const QString &hint, const QVariant &value)
{
    Q_D(Notification);
    d->hints.insert(hint, value);
}

/*!
    \qmlmethod void Notification::notifications()

    Returns a list of notifications created by the calling application.
    The returned objects are Notification components. They are only destroyed
    when the application is closed, so the caller should take their ownership
    and destroy them when they are not used anymore.
*/
QList<QObject*> Notification::notifications()
{
    // By default, only the notifications owned by us are returned
    return notifications(processName());
}

/*!
    \qmlmethod void Notification::notifications(const QString &owner)

    Returns a list of notifications matching the supplied \a owner.
    The returned objects are Notification components. They are only destroyed
    when the application is closed, so the caller should take their ownership
    and destroy them when they are not used anymore.
*/
QList<QObject*> Notification::notifications(const QString &owner)
{
    QList<NotificationData> notifications = notificationManager()->GetNotifications(owner);
    QList<QObject*> objects;
    foreach (const NotificationData &notification, notifications) {
        objects.append(createNotification(notification, notificationManager()));
    }
    return objects;
}

QVariant Notification::remoteAction(const QString &name, const QString &displayName,
                                    const QString &service, const QString &path, const QString &iface,
                                    const QString &method, const QVariantList &arguments)
{
    QVariantMap action;

    if (!name.isEmpty()) {
        action.insert(QStringLiteral("name"), name);
    }
    if (!displayName.isEmpty()) {
        action.insert(QStringLiteral("displayName"), displayName);
    }
    if (!service.isEmpty()) {
        action.insert(QStringLiteral("service"), service);
    }
    if (!path.isEmpty()) {
        action.insert(QStringLiteral("path"), path);
    }
    if (!iface.isEmpty()) {
        action.insert(QStringLiteral("iface"), iface);
    }
    if (!method.isEmpty()) {
        action.insert(QStringLiteral("method"), method);
    }
    if (!arguments.isEmpty()) {
        action.insert(QStringLiteral("arguments"), arguments);
    }

    return action;
}

Notification *Notification::createNotification(const NotificationData &data, QObject *parent)
{
    return new Notification(data, parent);
}

QDBusArgument &operator<<(QDBusArgument &argument, const NotificationData &data)
{
    argument.beginStructure();
    argument << data.appName;
    argument << data.replacesId;
    argument << data.appIcon;
    argument << data.summary;
    argument << data.body;
    argument << encodeActions(data.actions);
    argument << data.hints;
    argument << data.expireTimeout;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, NotificationData &data)
{
    QStringList tempStringList;

    argument.beginStructure();
    argument >> data.appName;
    argument >> data.replacesId;
    argument >> data.appIcon;
    argument >> data.summary;
    argument >> data.body;
    argument >> tempStringList;
    argument >> data.hints;
    argument >> data.expireTimeout;
    argument.endStructure();

    data.actions = decodeActions(tempStringList);

    return argument;
}

