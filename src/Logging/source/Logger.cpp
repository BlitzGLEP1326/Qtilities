/****************************************************************************
**
** Copyright (c) 2009-2010, Jaco Naude
**
** This file is part of Qtilities which is released under the following
** licensing options.
**
** Option 1: Open Source
** Under this license Qtilities is free software: you can
** redistribute it and/or modify it under the terms of the GNU General
** Public License as published by the Free Software Foundation, either
** version 3 of the License, or (at your option) any later version.
**
** Qtilities is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Qtilities. If not, see http://www.gnu.org/licenses/.
**
** Option 2: Commercial
** Alternatively, this library is also released under a commercial license
** that allows the development of closed source proprietary applications
** without restrictions on licensing. For more information on this option,
** please see the project website's licensing page:
** http://www.qtilities.org/licensing.html
**
** If you are unsure which license is appropriate for your use, please
** contact support@qtilities.org.
**
****************************************************************************/

#include "Logger.h"
#include "LoggerFactory.h"
#include "AbstractFormattingEngine.h"
#include "AbstractLoggerEngine.h"
#include "FormattingEngines.h"
#include "LoggerEngines.h"
#include "LoggingConstants.h"

#include <Qtilities.h>

#include <QtDebug>
#include <QMutex>

using namespace Qtilities::Logging::Constants;

struct Qtilities::Logging::LoggerData {
    LoggerFactory<AbstractLoggerEngine> logger_engine_factory;
    QList<AbstractLoggerEngine*> logger_engines;
    QList<AbstractFormattingEngine*> formatting_engines;
    QString default_formatting_engine;
    Logger::MessageType global_log_level;
    bool initialized;
    bool is_qt_message_handler;
    bool remember_session_config;
    QPointer<AbstractFormattingEngine> priority_formatting_engine;
};

Qtilities::Logging::Logger* Qtilities::Logging::Logger::m_Instance = 0;

Qtilities::Logging::Logger* Qtilities::Logging::Logger::instance()
{
    static QMutex mutex;
    if (!m_Instance)
    {
      mutex.lock();

      if (!m_Instance)
        m_Instance = new Logger;

      mutex.unlock();
    }

    return m_Instance;
}

Qtilities::Logging::Logger::Logger(QObject* parent) : QObject(parent)
{    
    d = new LoggerData;

    d->default_formatting_engine = QString("Uninitialized");
    d->global_log_level = Logger::Debug;
    d->initialized = false;
    d->remember_session_config = false;
    d->priority_formatting_engine = 0;
}

Qtilities::Logging::Logger::~Logger()
{
    clear();
    delete d;
}

void Qtilities::Logging::Logger::initialize() {
    if (d->initialized)
        return;

    // In the initialize function we use the Qt Debug logging system to log messages since no logger engines would be present at this stage.
    qDebug() << tr("Qtilities Logging Framework, initialization started...");

    // Register the formatting engines that comes as part of the Qtilities Logging Framework
    d->formatting_engines << &FormattingEngine_Default::instance();
    d->formatting_engines << &FormattingEngine_Rich_Text::instance();
    d->formatting_engines << &FormattingEngine_XML::instance();
    d->formatting_engines << &FormattingEngine_HTML::instance();
    d->formatting_engines << &FormattingEngine_QtMsgEngineFormat::instance();
    d->default_formatting_engine = QString(FORMATTING_ENGINE_DEFAULT);

    // Register the logger enigines that comes as part of the Qtilities Logging Framework
    d->logger_engine_factory.registerFactoryInterface(TAG_LOGGER_ENGINE_FILE, &FileLoggerEngine::factory);

    qDebug() << tr("> Number of formatting engines available: ") << d->formatting_engines.count();
    qDebug() << tr("> Number of logger engine factories available: ") << d->logger_engine_factory.tags().count();

    // Attach a QtMsgLoggerEngine and a ConsoleLoggerEngine and disable them both.
    AbstractLoggerEngine* tmp_engine_ptr = QtMsgLoggerEngine::instance();
    tmp_engine_ptr->installFormattingEngine(&FormattingEngine_QtMsgEngineFormat::instance());
    attachLoggerEngine(tmp_engine_ptr, true);
    toggleQtMsgEngine(false);

    tmp_engine_ptr = qobject_cast<AbstractLoggerEngine*> (ConsoleLoggerEngine::instance());
    tmp_engine_ptr->installFormattingEngine(&FormattingEngine_Default::instance());
    attachLoggerEngine(tmp_engine_ptr, true);
    toggleConsoleEngine(false);

    readSettings();

    // Now load the logger config if neccesarry.
    if (d->remember_session_config) {
        loadSessionConfig();
    }

    d->initialized = true;
    qDebug() << tr("Qtilities Logging Framework, initialization finished successfully...");
}

void Qtilities::Logging::Logger::finalize() {
    if (d->remember_session_config) {
        saveSessionConfig();
    }

    clear();
}

void Qtilities::Logging::Logger::clear() {
    // Delete all logger engines
    qDebug() << tr("Qtilities Logging Framework, clearing started...");
    for (int i = 0; i < d->logger_engines.count(); i++) {
        if (d->logger_engines.at(i) != QtMsgLoggerEngine::instance() && d->logger_engines.at(i) != ConsoleLoggerEngine::instance()) {
            qDebug() << tr("> Deleting logger engine: ") << d->logger_engines.at(i)->objectName();
            delete d->logger_engines.at(i);
        }
    }
    d->logger_engines.clear();
    qDebug() << tr("Qtilities Logging Framework, clearing finished successfully...");
}

void Qtilities::Logging::Logger::logMessage(const QString& engine_name, MessageType message_type, const QVariant& message, const QVariant &msg1, const QVariant &msg2, const QVariant &msg3, const QVariant &msg4, const QVariant &msg5, const QVariant &msg6, const QVariant &msg7, const QVariant &msg8 , const QVariant &msg9) {
    // In release mode we should not log debug and trace messages.
    #ifdef QT_NO_DEBUG
        if (message_type == Debug || message_type == Trace)
            return;
    #endif

    if (message_type == AllLogLevels || message_type == None)
        return;

    if (message_type > d->global_log_level)
        return;

    QList<QVariant> message_contents;
    message_contents.push_back(message);
    if (!msg1.isNull()) message_contents.push_back(msg1);
    if (!msg2.isNull()) message_contents.push_back(msg2);
    if (!msg3.isNull()) message_contents.push_back(msg3);
    if (!msg4.isNull()) message_contents.push_back(msg4);
    if (!msg5.isNull()) message_contents.push_back(msg5);
    if (!msg6.isNull()) message_contents.push_back(msg6);
    if (!msg7.isNull()) message_contents.push_back(msg7);
    if (!msg8.isNull()) message_contents.push_back(msg8);
    if (!msg9.isNull()) message_contents.push_back(msg9);

    emit newMessage(engine_name,message_type,message_contents);
}

void Qtilities::Logging::Logger::logPriorityMessage(const QString& engine_name, MessageType message_type, const QVariant& message, const QVariant &msg1, const QVariant &msg2, const QVariant &msg3, const QVariant &msg4, const QVariant &msg5, const QVariant &msg6, const QVariant &msg7, const QVariant &msg8 , const QVariant &msg9) {
    // In release mode we should not log debug and trace messages.
    #ifdef QT_NO_DEBUG
        if (message_type == Debug || message_type == Trace)
            return;
    #endif

    if (message_type == AllLogLevels || message_type == None)
        return;

    if (message_type > d->global_log_level)
        return;

    QList<QVariant> message_contents;
    message_contents.push_back(message);
    if (!msg1.isNull()) message_contents.push_back(msg1);
    if (!msg2.isNull()) message_contents.push_back(msg2);
    if (!msg3.isNull()) message_contents.push_back(msg3);
    if (!msg4.isNull()) message_contents.push_back(msg4);
    if (!msg5.isNull()) message_contents.push_back(msg5);
    if (!msg6.isNull()) message_contents.push_back(msg6);
    if (!msg7.isNull()) message_contents.push_back(msg7);
    if (!msg8.isNull()) message_contents.push_back(msg8);
    if (!msg9.isNull()) message_contents.push_back(msg9);

    emit newMessage(engine_name,message_type,message_contents);

    QString formatted_message;
    if (d->priority_formatting_engine) {
        formatted_message = d->priority_formatting_engine->formatMessage(message_type,message_contents);
    } else
        formatted_message = message.toString();

    emit newPriorityMessage(message_type,formatted_message);
}

bool Qtilities::Logging::Logger::setPriorityFormattingEngine(const QString& name) {
    if (!availableFormattingEngines().contains(name))
        return false;

    if (d->priority_formatting_engine)
        delete d->priority_formatting_engine;

    d->priority_formatting_engine = formattingEngineReference(name);
    return true;
}

void Qtilities::Logging::Logger::setPriorityFormattingEngine(AbstractFormattingEngine* engine) {
    if (engine)
        d->priority_formatting_engine = engine;
}

QStringList Qtilities::Logging::Logger::availableFormattingEngines() const {
    QStringList names;
    for (int i = 0; i < d->formatting_engines.count(); i++) {
        names << d->formatting_engines.at(i)->name();
    }
    return names;
}

Qtilities::Logging::AbstractFormattingEngine* Qtilities::Logging::Logger::formattingEngineReference(const QString& name) {
    for (int i = 0; i < d->formatting_engines.count(); i++) {
        if (name == d->formatting_engines.at(i)->name())
            return d->formatting_engines.at(i);
    }
    return 0;
}

Qtilities::Logging::AbstractFormattingEngine* Qtilities::Logging::Logger::formattingEngineReferenceFromExtension(const QString& file_extension) {
    for (int i = 0; i < d->formatting_engines.count(); i++) {
        if (file_extension == d->formatting_engines.at(i)->fileExtension())
            return d->formatting_engines.at(i);
    }
    return 0;
}

Qtilities::Logging::AbstractFormattingEngine* Qtilities::Logging::Logger::formattingEngineReferenceAt(int index) {
    if (index < 0 || index >= d->formatting_engines.count())
        return 0;

    return d->formatting_engines.at(index);
}

// Functions related to logger engine factories
Qtilities::Logging::AbstractLoggerEngine* Qtilities::Logging::Logger::newLoggerEngine(const QString& tag, AbstractFormattingEngine* formatting_engine) {
    // If an invalid tag is specified, an assertion will be triggered from within the factory itself.
    AbstractLoggerEngine* new_engine = d->logger_engine_factory.createInstance(tag);
    new_engine->setObjectName(tag);

    // Install a formatting engine for the new logger engine
    if (formatting_engine)
        new_engine->installFormattingEngine(formatting_engine);

    return new_engine;
}

void Qtilities::Logging::Logger::registerLoggerEngineFactory(const QString& tag, LoggerFactoryInterface<AbstractLoggerEngine>* factory_iface) {
    d->logger_engine_factory.registerFactoryInterface(tag, factory_iface);
}

QStringList Qtilities::Logging::Logger::availableLoggerEngines() const {
    return d->logger_engine_factory.tags();
}

int Qtilities::Logging::Logger::attachedFormattingEngineCount() const {
    return d->formatting_engines.count();
}

QString Qtilities::Logging::Logger::defaultFormattingEngine() const {
    return d->default_formatting_engine;
}

bool Qtilities::Logging::Logger::attachLoggerEngine(AbstractLoggerEngine* new_logger_engine, bool initialize_engine) {
    if (initialize_engine) {
        bool init_result = new_logger_engine->initialize();
        if (!init_result) {
            LOG_ERROR(tr("New file logger engine could not be added, it failed during initialization."));
            delete new_logger_engine;
            new_logger_engine = 0;
            return false;
        }
    }

    if (new_logger_engine) {
        d->logger_engines << new_logger_engine;
        connect(this,SIGNAL(newMessage(QString,Logger::MessageType,QList<QVariant>)),new_logger_engine,SLOT(newMessages(QString,Logger::MessageType,QList<QVariant>)));
    }

    emit loggerEngineCountChanged(new_logger_engine, EngineAdded);
    return true;
}

bool Qtilities::Logging::Logger::detachLoggerEngine(AbstractLoggerEngine* logger_engine) {
    if (logger_engine) {
        if (d->logger_engines.removeOne(logger_engine)) {
            emit loggerEngineCountChanged(logger_engine, EngineRemoved);
            delete logger_engine;
            return true;
        }
    }

    return false;
}

QString Qtilities::Logging::Logger::logLevelToString(Logger::MessageType log_level) const {
    if (log_level == None) {
        return "None";
    } else if (log_level == Info) {
        return "Information";
    } else if (log_level == Warning) {
        return "Warning";
    } else if (log_level == Error) {
        return "Error";
    } else if (log_level == Fatal) {
        return "Fatal";
    } else if (log_level == Debug) {
        return "Debug";
    } else if (log_level == Trace) {
        return "Trace";
    } else if (log_level == AllLogLevels) {
        return "All Log Levels";
    }

    return QString();
}

Qtilities::Logging::Logger::MessageType Qtilities::Logging::Logger::stringToLogLevel(const QString& log_level_string) const {
    if (log_level_string == "Information") {
        return Logger::Info;
    } else if (log_level_string == "Warning") {
        return Logger::Warning;
    } else if (log_level_string == "Error") {
        return Logger::Error;
    } else if (log_level_string == "Fatal") {
        return Logger::Fatal;
    } else if (log_level_string == "Debug") {
        return Logger::Debug;
    } else if (log_level_string == "Trace") {
        return Logger::Trace;
    } else if (log_level_string == "All Log Levels") {
        return Logger::AllLogLevels;
    }
    return Logger::None;
}

QStringList Qtilities::Logging::Logger::allLogLevelStrings() const {
    QStringList strings;
    strings << "None";
    strings << "Information";
    strings << "Warning";
    strings << "Error";
    strings << "Fatal";

    #ifndef QT_NO_DEBUG
        strings << "Debug";
        strings << "Trace";
    #endif
    strings << "All Log Levels";
    return strings;
}

void Qtilities::Logging::Logger::deleteAllLoggerEngines() {
    // Delete all logger engines
    for (int i = 0; i << d->logger_engines.count(); i++) {
        delete d->logger_engines.at(0);
    }
    d->logger_engines.clear();
}

void Qtilities::Logging::Logger::disableAllLoggerEngines() {
    for (int i = 0; i < d->logger_engines.count(); i++) {
        d->logger_engines.at(i)->setActive(false);
    }
}

void Qtilities::Logging::Logger::enableAllLoggerEngines() {
    for (int i = 0; i < d->logger_engines.count(); i++) {
        d->logger_engines.at(i)->setActive(true);
    }
}

void Qtilities::Logging::Logger::deleteEngine(const QString& engine_name) {
    QObject* engine = loggerEngineReference(engine_name);

    if (!engine)
        return;

    delete engine;
}

void Qtilities::Logging::Logger::enableEngine(const QString& engine_name) {
    AbstractLoggerEngine* engine = loggerEngineReference(engine_name);

    if (!engine)
        return;

    engine->setActive(false);
}

void Qtilities::Logging::Logger::disableEngine(const QString engine_name) {
    AbstractLoggerEngine* engine = loggerEngineReference(engine_name);

    if (!engine)
        return;

    engine->setActive(true);
}

QStringList Qtilities::Logging::Logger::attachedLoggerEngineNames() const {
    QStringList names;
    for (int i = 0; i < d->logger_engines.count(); i++) {
        names << d->logger_engines.at(i)->name();
    }
    return names;
}

int Qtilities::Logging::Logger::attachedLoggerEngineCount() const {
    return d->logger_engines.count();
}

Qtilities::Logging::AbstractLoggerEngine* Qtilities::Logging::Logger::loggerEngineReference(const QString& engine_name) {
    for (int i = 0; i < d->logger_engines.count(); i++) {
        if (d->logger_engines.at(i)) {
            if (engine_name == d->logger_engines.at(i)->name())
                return d->logger_engines.at(i);
        }
    }
    return 0;
}

Qtilities::Logging::AbstractLoggerEngine* Qtilities::Logging::Logger::loggerEngineReferenceAt(int index) {
    if (index < 0 || index >= d->logger_engines.count())
        return 0;

    return d->logger_engines.at(index);
}

void Qtilities::Logging::Logger::setGlobalLogLevel(Logger::MessageType new_log_level) {
    if (d->global_log_level == new_log_level)
        return;

    d->global_log_level = new_log_level;

    writeSettings();
    LOG_INFO("Global log level changed to " + logLevelToString(new_log_level));
}

Qtilities::Logging::Logger::MessageType Qtilities::Logging::Logger::globalLogLevel() const {
    return d->global_log_level;
}

void Qtilities::Logging::Logger::writeSettings() const {
    // Store settings using QSettings only if it was initialized
    QSettings settings;
    settings.beginGroup("Session Log");
    settings.beginGroup("General");
    settings.setValue("global_log_level", QVariant(d->global_log_level));
    settings.setValue("is_qt_message_handler", d->is_qt_message_handler);
    settings.setValue("remember_session_config", d->remember_session_config);
    settings.endGroup();
    settings.endGroup();
}

void Qtilities::Logging::Logger::readSettings() {
    if (QCoreApplication::organizationName().isEmpty() || QCoreApplication::organizationDomain().isEmpty() || QCoreApplication::applicationName().isEmpty())
        qDebug() << tr("The logger may not be able to restore paramaters from previous sessions since the correct details in QCoreApplication have not been set.");

    // Load logging paramaters using QSettings()
    QSettings settings;
    settings.beginGroup("Session Log");
    settings.beginGroup("General");
    QVariant log_level =  settings.value("global_log_level", Fatal);
    d->global_log_level = (MessageType) log_level.toInt();
    if (settings.value("is_qt_message_handler", false).toBool())
        installAsQtMessageHandler(false);
    if (settings.value("remember_session_config", true).toBool())
        d->remember_session_config = true;
    else
        d->remember_session_config = false;
    settings.endGroup();
    settings.endGroup();
}

void Qtilities::Logging::Logger::setRememberSessionConfig(bool remember) {
    if (d->remember_session_config == remember)
        return;

    d->remember_session_config = remember;
    writeSettings();
}

bool Qtilities::Logging::Logger::rememberSessionConfig() const {
    return d->remember_session_config;
}

void Qtilities::Logging::Logger::installAsQtMessageHandler(bool update_stored_settings) {
    qInstallMsgHandler(installLoggerMessageHandler);
    d->is_qt_message_handler = true;
    if (update_stored_settings)
        writeSettings();

    LOG_INFO("Capturing of Qt debug system messages is now enabled.");
}

void Qtilities::Logging::Logger::uninstallAsQtMessageHandler() {
    qInstallMsgHandler(0);
    d->is_qt_message_handler = false;
    writeSettings();

    LOG_INFO("Capturing of Qt debug system messages is now disabled.");
}

bool Qtilities::Logging::Logger::isQtMessageHandler() const {
    return d->is_qt_message_handler;
}

void Qtilities::Logging::Logger::setIsQtMessageHandler(bool toggle) {
    d->is_qt_message_handler = toggle;
    writeSettings();

    if (toggle) {
        qInstallMsgHandler(installLoggerMessageHandler);
        LOG_INFO("Capturing of Qt debug system messages is now enabled.");
    } else {
        qInstallMsgHandler(0);
        LOG_INFO("Capturing of Qt debug system messages is now disabled.");
    }
}

void Qtilities::Logging::installLoggerMessageHandler(QtMsgType type, const char *msg)
{
    static QMutex msgMutex;
    if(!msgMutex.tryLock())
        return;

    switch (type)
    {
    case QtDebugMsg:
        Log->logMessage(QString("All"),Logger::Debug, msg);
        break;
    case QtWarningMsg:
        Log->logMessage(QString("All"),Logger::Warning, msg);
        break;
    case QtCriticalMsg:
        Log->logMessage(QString("All"),Logger::Error, msg);
        break;
    case QtFatalMsg:
        Log->logMessage(QString("All"),Logger::Fatal, msg);
        msgMutex.unlock();
        abort();
    }

    msgMutex.unlock();
}

// ------------------------------------
// Convenience functions provided to create new engines
// ------------------------------------
bool Qtilities::Logging::Logger::newFileEngine(const QString& engine_name, const QString& file_name, const QString& formatting_engine) {
    if (file_name.isEmpty())
        return false;

    FileLoggerEngine* file_engine;
    AbstractLoggerEngine* new_engine = d->logger_engine_factory.createInstance("File");
    new_engine->setObjectName(engine_name);

    file_engine = qobject_cast<FileLoggerEngine*> (new_engine);
    file_engine->setFileName(file_name);

    // Install a formatting engine for the new logger engine
    if (formattingEngineReference(formatting_engine)) {
        AbstractFormattingEngine* formatting_engine_inst = formattingEngineReference(formatting_engine);
        if (!formatting_engine_inst) {
            delete new_engine;
            return false;
        }
        new_engine->installFormattingEngine(formatting_engine_inst);
    } else {
        // Attempt to get the formatting engine with the specified file format.
        QFileInfo fi(file_name);
        QString extension = fi.fileName().split(".").last();
        AbstractFormattingEngine* formatting_engine_inst = formattingEngineReferenceFromExtension(extension);
        if (!formatting_engine_inst) {
            delete new_engine;
            return false;
        }
        new_engine->installFormattingEngine(formatting_engine_inst);
    }

    if (attachLoggerEngine(new_engine, true)) {
        return true;
    } else {
        delete new_engine;
        return false;
    }
}

void Qtilities::Logging::Logger::toggleQtMsgEngine(bool toggle) {
    if (d->logger_engines.contains(QtMsgLoggerEngine::instance()))
        QtMsgLoggerEngine::instance()->setActive(toggle);
}

void Qtilities::Logging::Logger::toggleConsoleEngine(bool toggle) {
    if (d->logger_engines.contains(ConsoleLoggerEngine::instance()))
        ConsoleLoggerEngine::instance()->setActive(toggle);
}

quint32 MARKER_LOGGER_CONFIG_TAG = 0xFAC0000F;

bool Qtilities::Logging::Logger::saveSessionConfig(QString file_name) const {
    if (file_name.isEmpty())
        file_name = QCoreApplication::applicationDirPath() + PATH_LOG_LAST_CONFIG;

    LOG_DEBUG(tr("Logging configuration export started to ") + file_name);

    // Check if the directory exists:
    QFile file(file_name);
    if (!file.open(QIODevice::ReadWrite)) {
        LOG_DEBUG(tr("Logging configuration export failed to ") + file_name + tr(". The file could not be opened."));
        return false;
    }
    QDataStream stream(&file);   // we will serialize the data into the file
    stream << (quint32) QTILITIES_LOGGER_BINARY_EXPORT_FORMAT;

    // Stream exportable engines:
    QList<ILoggerExportable*> export_list;
    for (int i = 0; i < d->logger_engines.count(); i++) {
        ILoggerExportable* log_export_iface = qobject_cast<ILoggerExportable*> (d->logger_engines.at(i));
        if (log_export_iface)
            export_list.append(log_export_iface);
    }

    stream << MARKER_LOGGER_CONFIG_TAG;
    stream << (quint32) d->global_log_level;
    stream << (quint32) export_list.count();

    bool success = true;
    for (int i = 0; i < export_list.count(); i++) {
        if (success) {
            LOG_DEBUG(tr("Exporting factory instance: ") + export_list.at(i)->factoryTag());
            stream << export_list.at(i)->factoryTag();
            success = export_list.at(i)->exportBinary(stream);
        } else
            break;
    }

    // Stream activity and formatting engines of all current engines:
    if (success) {
        stream << (quint32) d->logger_engines.count();
        for (int i = 0; i < d->logger_engines.count(); i++) {
            LOG_DEBUG(tr("Saving properties for engine: ") + d->logger_engines.at(i)->name());
            stream << d->logger_engines.at(i)->name();
            stream << d->logger_engines.at(i)->formattingEngineName();
            stream << d->logger_engines.at(i)->isActive();
        }
    }

    // End properly:
    if (success) {
        stream << MARKER_LOGGER_CONFIG_TAG;
        file.close();
        LOG_INFO(tr("Logging configuration successfully exported to ") + file_name);
        return true;
    } else {
        file.close();
        LOG_INFO(tr("Logging configuration export failed to ") + file_name);
        return false;
    }
}

bool Qtilities::Logging::Logger::loadSessionConfig(QString file_name) {
    if (file_name.isEmpty())
        file_name = QCoreApplication::applicationDirPath() + PATH_LOG_LAST_CONFIG;

    LOG_DEBUG(tr("Logging configuration import started from ") + file_name);
    QFile file(file_name);
    file.open(QIODevice::ReadOnly);
    QDataStream stream(&file);   // we will serialize the data into the file

    quint32 ui32;
    stream >> ui32;
    LOG_INFO(QString(tr("Inspecting logger configuration file format: Found binary export file format version: %1")).arg(ui32));
    if (ui32 != (quint32) QTILITIES_LOGGER_BINARY_EXPORT_FORMAT) {
        LOG_ERROR(QString(tr("Logger configuration file format does not match the expected binary export file format (expected version: %1). Import will fail.")).arg(QTILITIES_LOGGER_BINARY_EXPORT_FORMAT));
        return false;
    }
    stream >> ui32;
    if (ui32 != MARKER_LOGGER_CONFIG_TAG) {
        file.close();
        LOG_INFO(tr("Logging configuration import failed from ") + file_name + tr(". The file contains invalid data or does not exist."));
        return false;
    }

    quint32 global_log_level;
    stream >> global_log_level;

    quint32 import_count;
    stream >> import_count;
    int import_count_int = import_count;

    // Create all engines:
    bool success = true;
    QList<AbstractLoggerEngine*> engine_list;
    for (int i = 0; i < import_count_int; i++) {
        if (!success)
            break;

        QString tag;
        stream >> tag;
        LOG_DEBUG(tr("Create factory instance: ") + tag);
        AbstractLoggerEngine* engine = d->logger_engine_factory.createInstance(tag);
        if (engine) {
            ILoggerExportable* log_export_iface = qobject_cast<ILoggerExportable*> (engine);
            if (log_export_iface) {
                log_export_iface->importBinary(stream);
                engine_list.append(engine);
            } else {
                success = false;
            }
        } else {
            success = false;
        }
    }

    // Now attach all created engines to the logger, or delete them if neccesarry:
    int count = engine_list.count();
    if (success) {
        // First clear all engines in the logger which have exportable interfaces:
        QList<AbstractLoggerEngine*> iface_list;
        for (int i = 0; i < d->logger_engines.count(); i++) {
            ILoggerExportable* log_export_iface = qobject_cast<ILoggerExportable*> (d->logger_engines.at(i));
            if (log_export_iface)
                iface_list.append(d->logger_engines.at(i));
        }
        for (int i = 0; i < iface_list.count(); i++) {
            detachLoggerEngine(iface_list.at(i));
        }

        for (int i = 0; i < count; i++) {
            if (!attachLoggerEngine(engine_list.at(i)))
                success = false;
        }
    } else {
        for (int i = 0; i < count; i++) {
            delete engine_list.at(0);
        }
    }

    // Restore activity and formatting engines of all logger engines:
    if (success) {
        stream >> import_count;
        int import_count_int = import_count;
        QString current_name;
        QString current_engine;
        bool is_active;
        for (int i = 0; i < import_count_int; i++) {
            if (!success)
                break;

            stream >> current_name;
            stream >> current_engine;
            stream >> is_active;

            // Now check if the engine with the name is present, if so we set it's properties:
            AbstractLoggerEngine* engine = loggerEngineReference(current_name);
            if (engine) {
                LOG_DEBUG(tr("Restoring properties for engine: ") + current_name);
                engine->installFormattingEngine(formattingEngineReference(current_engine));
                engine->setActive(is_active);
            }
        }
    }

    // Report the correct message:
    if (success) {
        file.close();
        setGlobalLogLevel((Logger::MessageType) global_log_level);
        LOG_INFO(tr("Logging configuration successfully imported from ") + file_name);
        return true;
    } else {
        file.close();
        LOG_INFO(tr("Logging configuration import failed from ") + file_name);
        return false;
    }
}
