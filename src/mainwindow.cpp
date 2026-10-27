#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "asciiview.h"
#include "statusdot.h"
#include "custombutton.h"
#include "iconfallback.h"
#include "jsonparser.h"

#include <QDesktopServices>
#include <QFileDialog>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QMessageBox>
#include <QPainter>
#include <QScreen>
#include <QUrl>
#include <QWindow>
#include <QTextEdit>
#include <QDialogButtonBox>

#include <algorithm>
#include <utility>

const int PRECISION_TOTALS_TO_STR = 2;
const int PRECISION_CAPACITY_TO_STR = 0;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , settings("qdiskinfo", "qdiskinfo")
    , initializing(true)
    , mbSymbol("MB"), gbSymbol("GB"), tbSymbol("TB"), pbSymbol("PB")
{
    ui->setupUi(this);

    defaultCentralPalette = ui->centralwidget->palette();
    defaultMinimumWidth = minimumWidth();
    defaultMaximumSize = maximumSize();

    locale = QLocale::system();

    buttonGroup = new QButtonGroup(this);
    buttonGroup->setExclusive(true);

    QWidget *containerWidget = ui->scrollAreaWidgetContents;
    horizontalLayout = new QHBoxLayout(containerWidget);
    horizontalLayout->setContentsMargins(0, 0, 0, 0);
    ui->scrollArea->setWidget(containerWidget);

    verticalLayout = ui->centralwidget->findChild<QVBoxLayout*>("verticalLayout");
    gridLayout = ui->centralwidget->findChild<QGridLayout*>("gridLayout");

    diskName = qobject_cast<QLabel *>(ui->centralwidget->findChild<QLabel*>("diskName"));
    temperatureValue = qobject_cast<QLabel *>(ui->centralwidget->findChild<QLabel*>("temperatureValueLabel"));
    healthStatusValue = qobject_cast<QLabel *>(ui->centralwidget->findChild<QLabel*>("healthStatusValueLabel"));

    temperatureLabelHorizontal = qobject_cast<QLabel *>(ui->centralwidget->findChild<QLabel*>("healthStatusLabelHorizozontal"));
    healthStatusLabelHorizontal = qobject_cast<QLabel *>(ui->centralwidget->findChild<QLabel*>("temperatureLabelHorizontal"));
    temperatureValueHorizontal = qobject_cast<QLabel *>(ui->centralwidget->findChild<QLabel*>("temperatureValueLabelHorizontal"));
    healthStatusValueHorizontal = qobject_cast<QLabel *>(ui->centralwidget->findChild<QLabel*>("healthStatusValueLabelHorizontal"));
    temperatureLabelVertical = qobject_cast<QLabel *>(ui->centralwidget->findChild<QLabel*>("temperatureLabel"));
    healthStatusLabelVertical = qobject_cast<QLabel *>(ui->centralwidget->findChild<QLabel*>("healthStatusLabel"));

    firmwareLineEdit = qobject_cast<QLineEdit *>(ui->centralwidget->findChild<QLineEdit*>("firmwareLineEdit"));
    serialNumberLineEdit = qobject_cast<QLineEdit *>(ui->centralwidget->findChild<QLineEdit*>("serialNumberLineEdit"));
    typeLineEdit = qobject_cast<QLineEdit *>(ui->centralwidget->findChild<QLineEdit*>("typeLineEdit"));
    protocolLineEdit = qobject_cast<QLineEdit *>(ui->centralwidget->findChild<QLineEdit*>("protocolLineEdit"));
    deviceNodeLineEdit = qobject_cast<QLineEdit *>(ui->centralwidget->findChild<QLineEdit*>("deviceNodeLineEdit"));

    totalReadsLineEdit = qobject_cast<QLineEdit *>(ui->centralwidget->findChild<QLineEdit*>("totalReadsLineEdit"));
    totalWritesLineEdit = qobject_cast<QLineEdit *>(ui->centralwidget->findChild<QLineEdit*>("totalWritesLineEdit"));
    rotationRateLineEdit = qobject_cast<QLineEdit *>(ui->centralwidget->findChild<QLineEdit*>("rotationRateLineEdit"));
    powerOnCountLineEdit = qobject_cast<QLineEdit *>(ui->centralwidget->findChild<QLineEdit*>("powerOnCountLineEdit"));
    powerOnHoursLineEdit = qobject_cast<QLineEdit *>(ui->centralwidget->findChild<QLineEdit*>("powerOnHoursLineEdit"));

    tableWidget = qobject_cast<QTableWidget *>(ui->centralwidget->findChild<QTableWidget*>("dataTable"));
    if (tableWidget) {
        defaultTablePalette = tableWidget->palette();
        defaultViewportAutoFillBackground = tableWidget->viewport()->autoFillBackground();
        defaultAlternatingRowColors = tableWidget->alternatingRowColors();
    }
    serialNumberLineEdit->setEchoMode(QLineEdit::Password);

    nextButton = ui->centralwidget->findChild<QPushButton*>("nextButton");
    prevButton = ui->centralwidget->findChild<QPushButton*>("previousButton");
    nextButton->setShortcut(QKeySequence::Forward);
    prevButton->setShortcut(QKeySequence::Back);

    connect(nextButton, &QPushButton::clicked, this, &MainWindow::onNextButtonClicked);
    connect(prevButton, &QPushButton::clicked, this, &MainWindow::onPrevButtonClicked);

    nextButton->setFocusPolicy(Qt::NoFocus);
    prevButton->setFocusPolicy(Qt::NoFocus);

    ui->actionSave_Image->setShortcut(QKeySequence::Save);
    ui->actionQuit->setShortcut(QKeySequence::Quit);
    ui->actionRescan_Refresh->setShortcut(QKeySequence::Refresh);

    menuDevice = ui->menuDevice;
    menuDisk = ui->menuDisk;

    selfTestMenu = new QMenu(tr("Start Self Test"), menuDevice);
    menuDevice->addMenu(selfTestMenu);
    selfTestMenu->setToolTipsVisible(true);

    selfTestLogAction = new QAction(tr("Self Test Log"), this);
    menuDevice->addAction(selfTestLogAction);

    disksGroup = new QActionGroup(this);
    disksGroup->setExclusive(true);

    goodColor = QColor(Qt::green);
    cautionColor = QColor(Qt::yellow);
    badColor = QColor(Qt::red);
    naColor = QColor(Qt::gray);

    statusLabel = new QLabel(ui->centralwidget);
    statusLabel->hide();

    infoLayout = ui->centralwidget->findChild<QHBoxLayout*>("info");
    if (infoLayout && infoLayout->count() > 1 && infoLayout->itemAt(1)) {
        infoSpacer = infoLayout->itemAt(1)->spacerItem();
    }
    if (gridLayout && gridLayout->itemAtPosition(2, 0)) {
        gridLeftSpacer = gridLayout->itemAtPosition(2, 0)->spacerItem();
    }
    if (verticalLayout) {
        for (int i = 0; i < verticalLayout->count(); ++i) {
            if (QSpacerItem *sp = verticalLayout->itemAt(i)->spacerItem()) {
                verticalSpacers.append(qMakePair(i, sp));
            }
        }
    }

    gridView = new GridView(this);
    gridView->setWindowFlag(Qt::Window);

    ui->actionIgnore_C4_Reallocation_Event_Count->setChecked(settings.value("IgnoreC4", true).toBool());
    ui->actionHEX->setChecked(settings.value("HEX", true).toBool());
    ui->actionUse_Fahrenheit->setChecked(settings.value("Fahrenheit", false).toBool());
    ui->actionCyclic_Navigation->setChecked(settings.value("CyclicNavigation", false).toBool());
    ui->actionUse_GB_instead_of_TB->setChecked(settings.value("UseGB", false).toBool());

    serialEchoAction = serialNumberLineEdit->addAction(
        themedIconOrFallback(QStringLiteral("visibility"), iconForegroundColor()),
        QLineEdit::TrailingPosition);
    connect(serialEchoAction, &QAction::triggered, this, [=]() {
        if (serialNumberLineEdit->echoMode() == QLineEdit::Password) {
            serialNumberLineEdit->setEchoMode(QLineEdit::Normal);
        } else if (serialNumberLineEdit->echoMode() == QLineEdit::Normal) {
            serialNumberLineEdit->setEchoMode(QLineEdit::Password);
        }
        updateIconFallbacks();
    });

    QPair<QStringList, QJsonArray> values = Utils.scanDevices(initializing);
    deviceOutputs = values.first;
    devices = values.second;
    if (!deviceOutputs.isEmpty()) {
        updateUI();
    }
    this->setFocus();
    initializing = false;

    initializeDefaultThemeState();

    themeBackgroundTimer.setSingleShot(true);
    themeBackgroundTimer.setInterval(50);
    connect(&themeBackgroundTimer, &QTimer::timeout, this, &MainWindow::updateThemeBackground);

    themeManager.reload();
    setupThemeMenu();
    const QString savedTheme = settings.value(QStringLiteral("Theme/Name")).toString();
    if (savedTheme.isEmpty()) {
#if INCLUDE_OPTIONAL_RESOURCES
        setThemeSelection(QStringLiteral("__builtin__"), false);
#else
        setThemeSelection(QStringLiteral("__none__"), false);
#endif
    } else if (savedTheme == QLatin1String("__builtin__")) {
#if INCLUDE_OPTIONAL_RESOURCES
        setThemeSelection(savedTheme, false);
#else
        setThemeSelection(QStringLiteral("__none__"), false);
#endif
    } else if (savedTheme != QLatin1String("__none__") && !themeManager.theme(savedTheme)) {
        settings.remove(QStringLiteral("Theme/Name"));
        setThemeSelection(QStringLiteral("__none__"), false);
    } else {
        setThemeSelection(savedTheme, false);
    }

    mbSymbol = locale.formattedDataSize(1 << 20, 1, QLocale::DataSizeTraditionalFormat).split(' ')[1];
    gbSymbol = locale.formattedDataSize(1 << 30, 1, QLocale::DataSizeTraditionalFormat).split(' ')[1];
    tbSymbol = locale.formattedDataSize(qint64(1) << 40, 1, QLocale::DataSizeTraditionalFormat).split(' ')[1];
    pbSymbol = locale.formattedDataSize(qint64(1) << 50, 1, QLocale::DataSizeTraditionalFormat).split(' ')[1];

    updateIconFallbacks();
}

MainWindow::~MainWindow()
{
    if (infoSpacer && infoLayout && infoLayout->indexOf(infoSpacer) < 0) {
        delete infoSpacer;
    }
    infoSpacer = nullptr;
    if (gridLeftSpacer && gridLayout && gridLayout->indexOf(gridLeftSpacer) < 0) {
        delete gridLeftSpacer;
    }
    gridLeftSpacer = nullptr;
    for (const QPair<int, QSpacerItem *> &entry : std::as_const(verticalSpacers)) {
        if (verticalLayout && verticalLayout->indexOf(entry.second) < 0) {
            delete entry.second;
        }
    }
    verticalSpacers.clear();
    if (characterSpacer && gridLayout && gridLayout->indexOf(characterSpacer) < 0) {
        delete characterSpacer;
    }
    characterSpacer = nullptr;
    delete ui;
}

void MainWindow::onNextButtonClicked()
{
    qsizetype currentIndex = buttonGroup->buttons().indexOf(buttonGroup->checkedButton());
    qsizetype nextIndex = (currentIndex + 1) % buttonGroup->buttons().size();
    buttonGroup->buttons().at(static_cast<int>(nextIndex))->click();
    updateNavigationButtons(nextIndex);
}

void MainWindow::onPrevButtonClicked()
{
    qsizetype currentIndex = buttonGroup->buttons().indexOf(buttonGroup->checkedButton());
    qsizetype prevIndex = (currentIndex - 1 + buttonGroup->buttons().size()) % buttonGroup->buttons().size();
    buttonGroup->buttons().at(static_cast<int>(prevIndex))->click();
    updateNavigationButtons(prevIndex);
}

void MainWindow::updateNavigationButtons(qsizetype currentIndex)
{
    const qsizetype totalButtons = buttonGroup->buttons().size();
    const bool isCyclic = ui->actionCyclic_Navigation->isChecked();

    prevButton->setEnabled( // We can use setVisible if we want to mimic CrystalDiskInfo
        (currentIndex > 0) || (isCyclic && totalButtons > 1)
    );

    nextButton->setEnabled(
        (currentIndex < totalButtons - 1) || (isCyclic && totalButtons > 1)
    );
}

void MainWindow::updateUI(const QString &currentDeviceName)
{
    mbSymbol = locale.formattedDataSize(1 << 20, 1, QLocale::DataSizeTraditionalFormat).split(' ')[1];
    gbSymbol = locale.formattedDataSize(1 << 30, 1, QLocale::DataSizeTraditionalFormat).split(' ')[1];
    tbSymbol = locale.formattedDataSize(qint64(1) << 40, 1, QLocale::DataSizeTraditionalFormat).split(' ')[1];
    pbSymbol = locale.formattedDataSize(qint64(1) << 50, 1, QLocale::DataSizeTraditionalFormat).split(' ')[1];

    QVector<DiskItem> diskItems;

    bool firstTime = true;
    bool isFahrenheit = ui->actionUse_Fahrenheit->isChecked();
    globalIsNvme = false;

    QString degreeSymbol;
    if (isFahrenheit) { // We don't do Kelvin
        degreeSymbol = "°F";
    } else {
        degreeSymbol = "°C";
    }

    QList<QAction*> oldActions = disksGroup->actions();
    for (QAction *action : std::as_const(oldActions)) {
        disksGroup->removeAction(action);
        menuDisk->removeAction(action);
        delete action;
    }

    int deviceToSelect = -1;

    for (int i = 0; i < devices.size(); ++i) {
        QJsonObject device = devices[i].toObject();
        QString deviceName = device["name"].toString();

        QString allOutput;
        if (i >= 0 && i < deviceOutputs.size()) {
            allOutput = deviceOutputs[i];
        }

        QJsonDocument localDoc = QJsonDocument::fromJson(allOutput.toUtf8());
        QJsonObject localObj = localDoc.object();

        QString protocol = localObj["device"].toObject()["protocol"].toString();
        bool isNvme = (protocol == "NVMe");
        bool isScsi = (protocol == "SCSI");
        bool isAta = (protocol == "ATA");

        QString modelName = localObj["model_name"].toString();

        if (isScsi) {
            modelName = localObj["scsi_model_name"].toString();
        }

        QJsonArray attributes = localObj["ata_smart_attributes"].toObject()["table"].toArray();
        QString temperature = "-- " + degreeSymbol;
        QJsonValue smartStatusValue = localObj.value("smart_status");
        bool healthPassed = localObj["smart_status"].toObject()["passed"].toBool();
        bool caution = false;
        bool bad = false;
        QString health;
        QColor healthColor;

        double diskCapacityMB = localObj.value("user_capacity").toObject().value("bytes").toDouble() / 1e6;
        int64_t diskCapacityMbI64 = static_cast<int64_t>(diskCapacityMB);
        bool useGB = ui->actionUse_GB_instead_of_TB->isChecked();
        QString diskCapacityString = getMbToPrettyString(diskCapacityMbI64, PRECISION_CAPACITY_TO_STR, useGB);

        QJsonObject temperatureObj = localObj["temperature"].toObject();
        int temperatureInt = temperatureObj["current"].toInt();
        if (temperatureInt > 0) {
            if (isFahrenheit) {
                int fahrenheit = static_cast<int>((temperatureInt * 9.0 / 5.0) + 32.0);
                temperature = QString::number(fahrenheit);
            } else {
                temperature = QString::number(temperatureInt);
            }
            temperature = temperature + " " + degreeSymbol;
        }

        QVector<QPair<QString, int>> nvmeSmartOrdered;
        if (isAta) {
            for (const QJsonValue &attr : std::as_const(attributes)) {
                QJsonObject attrObj = attr.toObject();
                if ((attrObj["id"] == 5 || attrObj["id"] == 197 || attrObj["id"] == 198 || (attrObj["id"] == 196 && !(ui->actionIgnore_C4_Reallocation_Event_Count->isChecked()))) && attrObj["raw"].toObject()["value"].toDouble() > 0) {
                    caution = true;
                }
                if (attrObj["thresh"].toInt() && (attrObj["value"].toInt() < attrObj["thresh"].toInt())) {
                    bad = true;
                }
            }
        } else if (isScsi) {
            QJsonObject scsiErrorCounterLog = localObj.value("scsi_error_counter_log").toObject();
            static const QString keys[] = {
                QStringLiteral("read"),
                QStringLiteral("write"),
                QStringLiteral("verify")
            };
            for (const QString& key : keys) {
                if (scsiErrorCounterLog.value(key).toObject().value("total_uncorrected_errors").toInt() != 0) {
                    caution = true;
                }
            }

            int scsiGrownDefectList = localObj.value("scsi_grown_defect_list").toInt();
            if (scsiGrownDefectList != 0) {
                caution = true;
                QMessageBox::warning(nullptr, tr("Critical Warning"), deviceName + ": " + tr("Grown Defect List") + ": " + QString::number(scsiGrownDefectList));
            }
        } else {
            JsonParser parser;
            nvmeSmartOrdered = parser.parse(allOutput);
            int row = 1;
            for (const QPair<QString, int> &pair : std::as_const(nvmeSmartOrdered)) {
                QString id = QString("%1").arg(row, 2, 16, QChar('0')).toUpper();
                int raw = pair.second;
                if (id == "01" && raw) {
                    bad = true;
                } else if (id == "03") {
                    int availableSpareThreshold = nvmeSmartOrdered.at(3).second;
                    if (availableSpareThreshold > 100) { // Thx to crystaldiskinfo for these workarounds
                        ;
                    } else if (raw == 0 && availableSpareThreshold == 0) {
                        ;
                    } else if (raw < availableSpareThreshold) {
                        bad = true;
                    } else if (availableSpareThreshold != 100 && (raw == availableSpareThreshold)) {
                        caution = true;
                    }
                } else if (id == "05" && raw >= 90) { // Make this configurable, currently hardcoded to 10%
                    caution = true;
                }

                ++row;
            }
        }

        if (healthPassed && !caution && !bad) {
            health = tr("Good");
            healthColor = goodColor;
        } else if (healthPassed && caution && !bad) {
            health = tr("Caution");
            healthColor = cautionColor;
        } else if ((bad || !healthPassed) && !modelName.isEmpty() && !smartStatusValue.isNull()){
            health = tr("Bad");
            healthColor = badColor;
        } else {
            health = tr("Unknown");
            healthColor = naColor;
        }

        CustomButton *button = new CustomButton(health, temperature, deviceName, healthColor, this);
        button->setToolTip(tr("Disk") + " " + QString::number(i) + " : " +  modelName + " : " + diskCapacityString);
        button->setProperty("deviceName", deviceName);

        buttonGroup->addButton(button);
        horizontalLayout->addWidget(button);

        QAction *diskAction = new QAction("(" + QString::number(i+1) + ") " + modelName + " " + diskCapacityString, this);
        diskAction->setCheckable(true);
        menuDisk->addAction(diskAction);
        disksGroup->addAction(diskAction);

        button->setCheckable(true);
        button->setAutoExclusive(true);

        qsizetype buttonIndex = buttonGroup->buttons().indexOf(button);

        diskItems.append({ deviceName, temperature, health });

        auto updateWindow = [=]() {
            if (isNvme) {
                populateWindow(localObj, health, nvmeSmartOrdered);
            } else {
                populateWindow(localObj, health);
            }
            updateNavigationButtons(buttonIndex);
        };

        connect(button, &QPushButton::clicked, this, [=]() {
            updateWindow();
            disksGroup->actions().at(static_cast<int>(buttonIndex))->setChecked(true);
            gridView->highlightDisk(buttonIndex);
            gridView->setActiveIndex(buttonIndex);
        });

        connect(diskAction, &QAction::triggered, this, [=]() {
            updateWindow();
            button->setChecked(true);
            gridView->highlightDisk(buttonIndex);
            gridView->setActiveIndex(buttonIndex);
        });

        if (firstTime) {
            globalObj = localObj;
            globalHealth = health;
            button->setChecked(true);
            diskAction->setChecked(true);
            gridView->setActiveIndex(0);
            firstTime = false;
            globalIsNvme = isNvme;
            if (isNvme) {
                globalNvmeSmartOrdered = nvmeSmartOrdered;
            }
        }

        if (!currentDeviceName.isEmpty() && deviceName == currentDeviceName) {
            deviceToSelect = i;
            globalObj = localObj;
            globalHealth = health;
            globalIsNvme = isNvme;
            if (isNvme) {
                globalNvmeSmartOrdered = nvmeSmartOrdered;
            }
            button->setChecked(true);
            diskAction->setChecked(true);
            firstTime = false;
        }

        connect(gridView, &GridView::diskSelected, this, [=](int selectedIndex) {
            if (selectedIndex >= 0 && selectedIndex < buttonGroup->buttons().size()) {
                QAbstractButton *gridButton = buttonGroup->buttons().at(selectedIndex);
                if (gridButton) {
                    gridButton->click();
                }
            }
        });
    }

    buttonStretch = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
    horizontalLayout->addSpacerItem(buttonStretch);

    if (globalIsNvme) {
        populateWindow(globalObj, globalHealth, globalNvmeSmartOrdered);
    } else {
        populateWindow(globalObj, globalHealth);
    }

    gridView->setDisks(diskItems);
    int activeIndex = deviceToSelect >= 0 ? deviceToSelect : 0;
    gridView->setActiveIndex(activeIndex);
    updateNavigationButtons(activeIndex);
}

void MainWindow::populateWindow(const QJsonObject &localObj, const QString &health, const QVector<QPair<QString, int>>& nvmeLogOrdered)
{
    QJsonArray attributes = localObj["ata_smart_attributes"].toObject()["table"].toArray();
    QJsonObject pollingMinutes = localObj["ata_smart_data"].toObject()["self_test"].toObject()["polling_minutes"].toObject();
    QJsonObject nvmeLog = localObj["nvme_smart_health_information_log"].toObject();
    QString modelName = localObj["model_name"].toString();
    QString firmwareVersion = localObj["firmware_version"].toString("----");
    double diskCapacityMB = localObj.value("user_capacity").toObject().value("bytes").toDouble() / 1e6;
    int64_t diskCapacityMbI64 = static_cast<int64_t>(diskCapacityMB);
    int powerCycleCountInt = localObj["power_cycle_count"].toInt(-1);
    QJsonObject temperatureObj = localObj["temperature"].toObject();
    int temperatureInt = temperatureObj["current"].toInt();
    int64_t totalMbWritesI64 = 0;
    int64_t totalMbReadsI64 = 0;
    bool useGB = ui->actionUse_GB_instead_of_TB->isChecked();

    QString diskCapacityString = getMbToPrettyString(diskCapacityMbI64, PRECISION_CAPACITY_TO_STR, useGB);

    QString totalReads;
    QString totalWrites;
    QString percentage = "";
    QString serialNumber = localObj["serial_number"].toString();
    QJsonObject deviceObj = localObj["device"].toObject();
    QString protocol = deviceObj["protocol"].toString();
    QString type = deviceObj["type"].toString();
    QString name = deviceObj["name"].toString();
    QJsonArray outputArray = localObj.value("smartctl").toObject()["output"].toArray();

    QJsonArray nvmeSelfTestsTable = localObj["nvme_self_test_log"].toObject()["table"].toArray();
    QJsonArray ataSelfTestsTable = localObj["ata_smart_self_test_log"].toObject()["standard"].toObject()["table"].toArray();

    QJsonObject scsiErrorCounterLog;

    bool isNvme = (protocol == "NVMe");
    bool isScsi = (protocol == "SCSI");

    if (isScsi) {
        scsiErrorCounterLog = localObj.value("scsi_error_counter_log").toObject();
        modelName = localObj["scsi_model_name"].toString();
        powerCycleCountInt = localObj["scsi_start_stop_cycle_counter"].toObject().value("accumulated_load_unload_cycles").toInt();
        firmwareVersion = localObj["scsi_revision"].toString("----");
        totalMbReadsI64 = scsiErrorCounterLog.value("read").toObject().value("gigabytes_processed").toString().split(",").first().toInt() * 1000;
        totalMbWritesI64 = scsiErrorCounterLog.value("write").toObject().value("gigabytes_processed").toString().split(",").first().toInt() * 1000;
    }

    bool nvmeHasSelfTest = false;

    auto createTablePopup = [=](QJsonArray selfTestsTable) {
        QWidget *popup = new QWidget();
        QTableWidget *selfTestsTableWidget = new QTableWidget();
        selfTestsTableWidget->setFocusPolicy(Qt::NoFocus);

        qsizetype rowCount = selfTestsTable.size();
        if (rowCount > std::numeric_limits<int>::max()) {
            rowCount = std::numeric_limits<int>::max();
        }

        selfTestsTableWidget->setRowCount(static_cast<int>(rowCount));
        selfTestsTableWidget->setColumnCount(3);
        selfTestsTableWidget->verticalHeader()->setVisible(false);
        selfTestsTableWidget->setHorizontalHeaderLabels({tr("Type"), tr("Status"), tr("Power On Hours")});

        for (int i = 0; i < rowCount; ++i) {
            QJsonObject entry = selfTestsTable[i].toObject();
            QTableWidgetItem *item;

            if (isNvme) {
                selfTestsTableWidget->setItem(i, 0, new QTableWidgetItem(entry["self_test_code"].toObject()["string"].toString()));
                selfTestsTableWidget->setItem(i, 1, new QTableWidgetItem(entry["self_test_result"].toObject()["string"].toString()));
                item = new QTableWidgetItem(QString::number(entry["power_on_hours"].toInt()));
            } else {
                selfTestsTableWidget->setItem(i, 0, new QTableWidgetItem(entry["type"].toObject()["string"].toString()));
                selfTestsTableWidget->setItem(i, 1, new QTableWidgetItem(entry["status"].toObject()["string"].toString()));
                item = new QTableWidgetItem(QString::number(entry["lifetime_hours"].toInt()));
            }
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            selfTestsTableWidget->setItem(i, 2, item);
        }

        for (int i = 0; i < selfTestsTableWidget->columnCount(); ++i) {
            QTableWidgetItem *headerItem = selfTestsTableWidget->horizontalHeaderItem(i);
            if (headerItem) {
                if (i == 2) {
                   headerItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                } else {
                    headerItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                }
            }
        }


        selfTestsTableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        for (int i = 0; i < selfTestsTableWidget->columnCount(); ++i) {
            if (i != 1) {
                selfTestsTableWidget->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
            }
        }

        selfTestsTableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        selfTestsTableWidget->verticalHeader()->setDefaultSectionSize(31);
        selfTestsTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget(selfTestsTableWidget);
        popup->setLayout(layout);
        popup->setWindowTitle(tr("Self Test Log"));
        popup->resize(400, 400);
        popup->show();
    };

    deviceJson = localObj;

    diskName->setText("<html><head/><body><p><span style='font-size:14pt; font-weight:bold;'>" + modelName + " " + diskCapacityString + "</span></p></body></html>");
    firmwareLineEdit->setText(firmwareVersion);
    serialNumberLineEdit->setText(serialNumber);
    typeLineEdit->setText(type);
    protocolLineEdit->setText(protocol);
    deviceNodeLineEdit->setText(name);

    QStringList keys = pollingMinutes.keys();
    std::sort(keys.begin(), keys.end(), [&pollingMinutes](const QString& key1, const QString& key2) {
        return pollingMinutes[key1].toInt() < pollingMinutes[key2].toInt();
    });

    int rotationRateInt = localObj["rotation_rate"].toInt(-1);
    QString rotationRate;
    if (rotationRateInt > 0) {
        rotationRate = QString::number(rotationRateInt);
    } else if (rotationRateInt == 0 || isNvme) {
        rotationRate = "---- (SSD)";
    } else {
        rotationRate = "----";
    }

    rotationRateLineEdit->setText(rotationRate);
    rotationRateLineEdit->setAlignment(Qt::AlignRight);

    QString powerCycleCount;
    if (powerCycleCountInt >= 0) {
        powerCycleCount = QString::number(powerCycleCountInt) + " " + tr("count");
    } else {
        powerCycleCount = "Unknown";
    }

    powerOnCountLineEdit->setText(powerCycleCount);
    powerOnCountLineEdit->setAlignment(Qt::AlignRight);

    int powerOnTimeInt = localObj["power_on_time"].toObject().value("hours").toInt(-1);
    QString powerOnTime;
    if (powerOnTimeInt >= 0) {
        powerOnTime = QString::number(powerOnTimeInt) + " " + tr("hours");
    } else {
        powerOnTime = "Unknown";
    }

    powerOnHoursLineEdit->setText(powerOnTime);
    powerOnHoursLineEdit->setAlignment(Qt::AlignRight);

    int logicalBlockSize = localObj["logical_block_size"].toInt();
    if (!isNvme) {
        for (const QJsonValue &attr : std::as_const(attributes)) {
            QJsonObject attrObj = attr.toObject();
            if (attrObj["id"] == 202) {
                if (attrObj["name"] == "Percent_Lifetime_Remain") {
                    int percentageUsed = 100 - attrObj["raw"].toObject()["value"].toInt();
                    percentage = QString::number(percentageUsed) + " %";
                }
            } else if (attrObj["id"] == 241) {
                if (attrObj["name"] == "Total_Writes_GB") {
                    int gigabytes = attrObj["raw"].toObject()["value"].toInt();
                    totalMbWritesI64 = gigabytes * 1000;
                } else if (attrObj["name"] == "Host_Writes_32MiB") {
                    double megabytes = (attrObj["raw"].toObject()["value"].toInt() * 32 * 1024.0 * 1024.0) / 1e6;
                    totalMbWritesI64 = static_cast<int64_t>(megabytes);
                } else if (attrObj["name"] == "Host_Writes_GiB" || attrObj["name"] == "Lifetime_Writes_GiB") {
                    double gibibytes = attrObj["raw"].toObject()["value"].toDouble();
                    double bytesPerGiB = static_cast<double>(1ULL << 30);
                    double bytesPerMB = 1e6;
                    double conversionFactor = bytesPerGiB / bytesPerMB;
                    double megabytes = gibibytes * conversionFactor;
                    totalMbWritesI64 = static_cast<int64_t>(megabytes);
                } else if (attrObj["name"] == "Total_LBAs_Written") {
                    qlonglong lbaWritten = attrObj["raw"].toObject()["value"].toVariant().toLongLong();
                    double megabytes = double(logicalBlockSize * lbaWritten) / 1e6;
                    totalMbWritesI64 = static_cast<int64_t>(megabytes);
                    // workaround: for drives that report different value with bad attribute name
                    // Check if total MB writes are less than power on hours
                    if (totalMbWritesI64 / powerOnTimeInt == 0) {
                        // Assume it's 32MiB chunks reported as LBAs
                        megabytes = (double(lbaWritten * 32) * 1024.0 * 1024.0) / 1e6;
                        totalMbWritesI64 = static_cast<int64_t>(megabytes);
                    }
                }
            } else if (attrObj["id"] == 242) {
                if (attrObj["name"] == "Total_Reads_GB") {
                    int gigabytes = attrObj["raw"].toObject()["value"].toInt();
                    totalMbReadsI64 = gigabytes * 1000;
                } else if (attrObj["name"] == "Host_Reads_32MiB") {
                    double megabytes = (attrObj["raw"].toObject()["value"].toInt() * 32 * 1024 * 1024) / 1e6;
                    totalMbReadsI64 = static_cast<int64_t>(megabytes);
                } else if (attrObj["name"] == "Host_Reads_GiB" || attrObj["name"] == "Lifetime_Reads_GiB") {
                    double gibibytes = attrObj["raw"].toObject()["value"].toDouble();
                    double bytesPerGiB = static_cast<double>(1ULL << 30);
                    double bytesPerMB = 1e6;
                    double conversionFactor = bytesPerGiB / bytesPerMB;
                    double megabytes = gibibytes * conversionFactor;
                    totalMbReadsI64 = static_cast<int64_t>(megabytes);
                } else if (attrObj["name"] == "Total_LBAs_Read") {
                    qlonglong lbaRead = attrObj["raw"].toObject()["value"].toVariant().toLongLong();
                    double megabytes = double(logicalBlockSize * lbaRead) / 1e6;
                    totalMbReadsI64 = static_cast<int64_t>(megabytes);
                    // workaround for drives that report different value with bad attribute name
                    // Check if total MB reads are less than power on hours
                    if (totalMbReadsI64 / powerOnTimeInt == 0) {
                        // Assume it's 32MiB chunks reported as LBAs
                        megabytes = (double(lbaRead * 32) * 1024.0 * 1024.0) / 1e6;
                        totalMbReadsI64 = static_cast<int64_t>(megabytes);
                    }
                }
            } else if (attrObj["id"] == 246) { // MX500
                if (attrObj["name"] == "Total_LBAs_Written") {
                    qlonglong lbaWritten = attrObj["raw"].toObject()["value"].toVariant().toLongLong();
                    double megabytes = double(lbaWritten * logicalBlockSize) / 1e6;
                    totalMbWritesI64 = static_cast<int64_t>(megabytes);
                }
            } else if (attrObj["name"] == "Remaining_Lifetime_Perc") {
                int percentageUsed = attrObj["raw"].toObject()["value"].toInt();
                percentage = QString::number(percentageUsed) + " %";
            } else if (attrObj["name"] == "Percent_Lifetime_Remain") {
                int percentageUsed = attrObj["value"].toInt();
                percentage = QString::number(percentageUsed) + " %";
            } else if (attrObj["name"] == "Media_Wearout_Indicator" || attrObj["name"] == "SSD_Life_Left") {
                int percentageUsed = attrObj["value"].toInt();
                int percentageUsedWorst = attrObj["worst"].toInt();
                if (percentageUsedWorst <= 100) {
                    percentage = QString::number(percentageUsed) + " %";
                }
            }
        }

		// Some devices don't report or report incorrect reads/writes in legacy
		// attributes, so we look for that data in ata_device_statistics.
		// We do this after parsing through the legacy attributes
		// to override possibly incorrect calculations.
		QJsonObject ata_device_statistics = localObj["ata_device_statistics"].toObject();
		if (!ata_device_statistics.empty())
		{
			QJsonArray pages = ata_device_statistics["pages"].toArray();
			for (const QJsonValue &pageval : std::as_const(pages))
			{
				QJsonObject page = pageval.toObject();
				if (page["name"] == "General Statistics")
				{
					QJsonArray general_statistics = page["table"].toArray();
					for (const QJsonValue &stat : std::as_const(general_statistics))
					{
						QJsonObject statObj = stat.toObject();
						QString key = statObj["name"].toString();
						QJsonValue value = statObj["value"];
						
						if (key == "Logical Sectors Read") {
							double logicalSectorsRead = value.toDouble();
							double megabytes = logicalSectorsRead * logicalBlockSize / 1e6;
							totalMbReadsI64 = static_cast<int64_t>(megabytes);
						} else if (key == "Logical Sectors Written") {
							double logicalSectorsWritten = value.toDouble();
							double megabytes = logicalSectorsWritten * logicalBlockSize / 1e6;
							totalMbWritesI64 = static_cast<int64_t>(megabytes);
						}
					}
					break; // no need looking into other pages
				}
			}
		}
		
        if (percentage.isEmpty() && rotationRate == "---- (SSD)") { // Workaround for some drives which have this and another attribute
            for (const QJsonValue &attr : std::as_const(attributes)) {
                QJsonObject attrObj = attr.toObject();
                if (attrObj["name"] == "Wear_Leveling_Count") {
                    int percentageUsed = attrObj["value"].toInt();
                    percentage = QString::number(percentageUsed) + " %";
                    break;
                }
            }
        }
    } else {
        for (auto smartItem = nvmeLog.begin(); smartItem != nvmeLog.end(); ++smartItem) {
            QString key = smartItem.key();
            QJsonValue value = smartItem.value();
            // it appears that NVME drives report data_units as logical_block_size*1000 bytes
            if (key == "data_units_written") {
                double dataUnitsWritten = value.toDouble();
                double megabytes = (dataUnitsWritten * logicalBlockSize) / 1000;
                totalMbWritesI64 = static_cast<int64_t>(megabytes);
            } else if (key == "data_units_read") {
                double dataUnitsRead = value.toDouble();
                double megabytes = (dataUnitsRead * logicalBlockSize) / 1000;
                totalMbReadsI64 = static_cast<int64_t>(megabytes);
            } else if (key == "percentage_used") {
                int percentageUsed = 100 - value.toInt();
                percentage = QString::number(percentageUsed) + " %";
            }
        }
    }

    totalReads = getMbToPrettyString(totalMbReadsI64, PRECISION_TOTALS_TO_STR, useGB);
    totalWrites = getMbToPrettyString(totalMbWritesI64, PRECISION_TOTALS_TO_STR, useGB);

    totalReadsLineEdit->setText(totalReads);
    totalReadsLineEdit->setAlignment(Qt::AlignRight);

    totalWritesLineEdit->setText(totalWrites);
    totalWritesLineEdit->setAlignment(Qt::AlignRight);

    int warningTemperature = 55;
    int criticalTemperature = 60;

    if (isNvme) {
        QString stringValue;
        warningTemperature = 65;
        criticalTemperature = 70;
        for (const QJsonValue &value : std::as_const(outputArray)) {
            stringValue = value.toString();
            if (stringValue.startsWith("Optional Admin Commands")) {
                if (stringValue.contains("Self_Test")) {
                    nvmeHasSelfTest = true;
                }
            } else if (stringValue.startsWith("Warning  Comp. Temp. Threshold")) {
                qsizetype pos = stringValue.indexOf(':');
                if (pos != -1) {
                    QString thresholdStr = stringValue.mid(static_cast<int>(pos + 1)).trimmed();
                    int temperature = thresholdStr.section(' ', 0, 0).toInt();
                    if (temperature > 0) {
                        warningTemperature = temperature;
                    }
                }
            } else if (stringValue.startsWith("Critical Comp. Temp. Threshold")) {
                qsizetype pos = stringValue.indexOf(':');
                if (pos != -1) {
                    QString thresholdStr = stringValue.mid(static_cast<int>(pos + 1)).trimmed();
                    int temperature = thresholdStr.section(' ', 0, 0).toInt();
                    if (temperature > 0) {
                        criticalTemperature = temperature;
                    }
                }
            }
        }
    } else if (isScsi) {
        int driveTripInt = temperatureObj["drive_trip"].toInt();
        if (driveTripInt > 0) {
            criticalTemperature = driveTripInt;
        }
    }

    QString tempStyle;
    if (temperatureInt > warningTemperature) { // TODO: Let the user set an alarm temp.
        tempStyle = "background-color: " + badColor.name() + ";";
    } else if ((temperatureInt < criticalTemperature) && (temperatureInt > warningTemperature)){
        tempStyle = "background-color: " + cautionColor.name() + ";";
    } else if (temperatureInt == 0) {
        tempStyle = "background-color: " + naColor.name() + ";";
    } else {
        tempStyle = "background-color: " + goodColor.name() + ";";
    }

    const QString labelStyle = "font-size:12pt; font-weight:700; color:" + themeButtonTextColor();

    QString tempText;
    if (temperatureInt > 0) {
        if (ui->actionUse_Fahrenheit->isChecked()) {
            int fahrenheit = static_cast<int>((temperatureInt * 9.0 / 5.0) + 32.0);
            tempText = "<html><head/><body><p><span style='" + labelStyle +"'>" + QString::number(fahrenheit) + " °F</span></p></body></html>";
        } else {
            tempText = "<html><head/><body><p><span style='" + labelStyle +"'>" + QString::number(temperatureInt) + " °C</span></p></body></html>";
        }
    } else {
        tempText = "<html><head/><body><p><span style='" + labelStyle +"'>" + "-- °C" + "</span></p></body></html>";
    }

    if (temperatureValue) {
        temperatureValue->setStyleSheet(tempStyle);
        temperatureValue->setText(tempText);
        temperatureValue->setAlignment(Qt::AlignCenter);
    }
    if (temperatureValueHorizontal) {
        temperatureValueHorizontal->setStyleSheet(tempStyle);
        temperatureValueHorizontal->setText(tempText);
        temperatureValueHorizontal->setAlignment(Qt::AlignCenter);
    }

    QString healthStyle;
    if (health == tr("Bad")) {
        healthStyle = "background-color: " + badColor.name() + ";";
    } else if (health == tr("Caution")){
        healthStyle = "background-color: " + cautionColor.name() + ";";
    } else if (health == tr("Good")) {
        healthStyle = "background-color: " + goodColor.name() + ";";
    } else {
        healthStyle = "background-color: " + naColor.name() + ";";
    }

    updateStatusIllustration(health);

    QString percentageText = "";
    if (!percentage.isEmpty()) {
        if (themeLayoutActive) {
            percentageText = "  (" + percentage + ")"; // 2 spaces is not a mistake
        } else {
            percentageText = "<br>" + percentage;
        }
    }

    const QString healthText = "<html><head/><body><p><span style='" + labelStyle +"'>" + health + percentageText + "</span></p></body></html>";

    if (healthStatusValue) {
        healthStatusValue->setStyleSheet(healthStyle);
        healthStatusValue->setText(healthText);
        healthStatusValue->setAlignment(Qt::AlignCenter);
    }
    if (healthStatusValueHorizontal) {
        healthStatusValueHorizontal->setStyleSheet(healthStyle);
        healthStatusValueHorizontal->setText(healthText);
        healthStatusValueHorizontal->setAlignment(Qt::AlignCenter);
    }

    if (protocol == "ATA") {
        addSmartAttributesTable(attributes);
        selfTestMenu->clear();
        ui->actionASCII_View->setEnabled(true);

        if (keys.isEmpty()) {
            selfTestMenu->setDisabled(true);
        } else {
            selfTestMenu->setEnabled(true);
        }

        for (const QString& key : std::as_const(keys)) {
            QString minutes = QString::number(pollingMinutes[key].toInt());
            QString keyTranslated;
            if (key == "short") {
                keyTranslated = tr("Short");
            } else if (key == "conveyance") {
                keyTranslated = tr("Conveyance");
            } else if (key == "extended") {
                keyTranslated = tr("Extended");
            } else {
                keyTranslated = key;
                keyTranslated[0] = keyTranslated[0].toUpper();
            }
            QString actionLabel = keyTranslated + " (" + minutes + " " + tr("Min.)");
            QAction *action = new QAction(actionLabel, this);
            selfTestMenu->addAction(action);

            QString mode;
            if (key == "extended") {
                mode = "long";
            } else {
                mode = key;
            }

            connect(action, &QAction::triggered, this, [this, mode, name, minutes]() {
                Utils.selfTestHandler(mode, name, minutes);
            });
        }

        if (ataSelfTestsTable.isEmpty()) {
            selfTestLogAction->setDisabled(true);
        } else {
            selfTestLogAction->setEnabled(true);
            selfTestLogAction->disconnect();
            connect(selfTestLogAction, &QAction::triggered, this, [=]() {
                createTablePopup(ataSelfTestsTable);
            });
        }
    } else if (protocol == "SCSI") {
            selfTestMenu->clear();
            ui->actionASCII_View->setEnabled(true);
            selfTestMenu->setDisabled(true);
            selfTestLogAction->setDisabled(true);
            addSCSIErrorCounterLogTable(scsiErrorCounterLog);
    } else {
        addNvmeLogTable(nvmeLogOrdered);
        selfTestMenu->clear();
        ui->actionASCII_View->setDisabled(true); // TODO: Implement this for NVMe drives too

        if (nvmeHasSelfTest) {
            selfTestMenu->setEnabled(true);
        } else {
            selfTestMenu->setDisabled(true);
        }

        QAction *actionShort = new QAction(tr("Short"), this);
        selfTestMenu->addAction(actionShort);
        connect(actionShort, &QAction::triggered, this, [this, mode = "short", name, minutes = "0"]() {
            Utils.selfTestHandler(mode, name, minutes);
        });

        QAction *actionLong = new QAction(tr("Extended"), this);
        selfTestMenu->addAction(actionLong);
        connect(actionLong , &QAction::triggered, this, [this, mode = "long", name, minutes = "0"]() {
            Utils.selfTestHandler(mode, name, minutes);
        });

        if (nvmeSelfTestsTable.isEmpty()) {
            selfTestLogAction->setDisabled(true);
        } else {
            selfTestLogAction->setEnabled(true);
            selfTestLogAction->disconnect();
            connect(selfTestLogAction, &QAction::triggered, this, [=]() {
                createTablePopup(nvmeSelfTestsTable);
            });
        }
    }
}

void MainWindow::addSCSIErrorCounterLogTable(const QJsonObject &scsiErrorLog)
{

    QStringList operations = {"read", "write", "verify"};

    tableWidget->setRowCount(3);
    tableWidget->setColumnCount(9);
    tableWidget->verticalHeader()->setVisible(false);
    tableWidget->setItemDelegateForColumn(0, new StatusDot(tableWidget));

    tableWidget->setHorizontalHeaderLabels({
        "",
        "Operation",
        "Errors Corrected\nby ECC Fast",
        "Errors Corrected\nby ECC Delayed",
        "Errors Corrected\nby Rereads/Rewrites",
        "Total Errors\nCorrected",
        "Correction Algorithm\nInvocations",
        "Gigabytes\nProcessed",
        "Total Uncorrected\nErrors"
    });

    int row = 0;
    for (const QString& operation : operations) {
        QColor statusColor;
        QJsonObject operationData = scsiErrorLog[operation].toObject();

        if (operationData["total_uncorrected_errors"].toInt() == 0) {
            statusColor = goodColor;
        } else {
            statusColor = cautionColor;
        }

        QTableWidgetItem *statusItem = new QTableWidgetItem();
        statusItem->setData(Qt::BackgroundRole, QVariant(statusColor));

        tableWidget->setItem(row, 0, statusItem);

        QString operationTranslated;
        if (operation == "read") {
            operationTranslated = tr("Read");
        } else if (operation == "write") {
            operationTranslated = tr("Write");
        } else if (operation == "verify") {
            operationTranslated = tr("Verify");
        }

        tableWidget->setItem(row, 1, new QTableWidgetItem(operationTranslated));
        tableWidget->setItem(row, 2, new QTableWidgetItem(QString::number(operationData["errors_corrected_by_eccfast"].toInt())));
        tableWidget->setItem(row, 3, new QTableWidgetItem(QString::number(operationData["errors_corrected_by_eccdelayed"].toInt())));
        tableWidget->setItem(row, 4, new QTableWidgetItem(QString::number(operationData["errors_corrected_by_rereads_rewrites"].toInt())));
        tableWidget->setItem(row, 5, new QTableWidgetItem(QString::number(operationData["total_errors_corrected"].toInt())));
        tableWidget->setItem(row, 6, new QTableWidgetItem(QString::number(operationData["correction_algorithm_invocations"].toInt())));
        tableWidget->setItem(row, 7, new QTableWidgetItem(operationData["gigabytes_processed"].toString()));
        tableWidget->setItem(row, 8, new QTableWidgetItem(QString::number(operationData["total_uncorrected_errors"].toInt())));

        ++row;

        for (int rowNum = 0; rowNum < 3; ++rowNum) {
            for (int column = 0; column < 9; ++column) {
                QTableWidgetItem *item = tableWidget->item(rowNum, column);
                if (item) {
                    if (column == 1) {
                        item->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
                    } else {
                        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    }
                }
            }
        }
    }

    tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    for (int i = 0; i < tableWidget->columnCount(); ++i) {
        if (i != 1) {
            tableWidget->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
        }
    }

    tableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    tableWidget->verticalHeader()->setDefaultSectionSize(31);
    tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void MainWindow::addNvmeLogTable(const QVector<QPair<QString, int>>& nvmeLogOrdered)
{
    QString warningMessage = "";

    qsizetype rowCount = nvmeLogOrdered.size();
    if (rowCount > std::numeric_limits<int>::max()) {
        rowCount = std::numeric_limits<int>::max();
    }

    tableWidget->setRowCount(static_cast<int>(rowCount));
    tableWidget->setColumnCount(4);
    tableWidget->setHorizontalHeaderLabels({"", tr("ID"), tr("Attribute Name"), tr("Raw Values")});
    tableWidget->verticalHeader()->setVisible(false);
    tableWidget->setItemDelegateForColumn(0, new StatusDot(tableWidget));


    tableWidget->horizontalHeaderItem(2)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    tableWidget->horizontalHeaderItem(3)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

    int row = 0;
    for (const QPair<QString, int> &pair : std::as_const(nvmeLogOrdered)) {
        QString id = QString("%1").arg(row + 1, 2, 16, QChar('0')).toUpper();

        QString key = pair.first;
        QString name = key.replace("_", " ");
        if (name == "nsid") { // smartctl now adds an nsid field, skip it for now
            continue;
        }
        name = Utils.toTitleCase(name);

        int rawInt = pair.second;
        QString raw = QString::number(rawInt);
        if ((ui->actionHEX->isChecked())) {
            raw = QString("%1").arg(raw.toUInt(nullptr), 14, 16, QChar('0')).toUpper();
        }

        QColor statusColor;

        if (id == "01" && rawInt) {
            statusColor = badColor;
            if (rawInt == 1) { // Still need to figure out if this is DEC or HEX in JSON
                warningMessage = tr("Available spare capacity has fallen below the threshold"); // THX to CrystalDiskInfo for the messages
            } else if (rawInt == 2) {
                warningMessage = tr("Temperature error (Overheat or Overcool)");
            } else if (rawInt == 4) {
                warningMessage = tr("NVM subsystem reliability has been degraded");
            } else if (rawInt == 8) {
                warningMessage = tr("Media has been placed in Read Only Mode");
            } else if (rawInt == 10) {
                warningMessage = tr("Volatile memory backup device has Failed");
            } else if (rawInt == 20) {
                warningMessage = tr("Persistent memory region has become Read-Only");
            }
        } else if (id == "03") {
            int availableSpareThreshold = nvmeLogOrdered.at(3).second;
            if (availableSpareThreshold > 100) { // Thx to crystaldiskinfo for these workarounds
                statusColor = goodColor;
            } else if (rawInt == 0 && availableSpareThreshold == 0) {
                statusColor = goodColor;
            } else if (rawInt < availableSpareThreshold) {
                statusColor = badColor;
            } else if (availableSpareThreshold != 100 && (rawInt == availableSpareThreshold)) {
                statusColor = cautionColor;
            } else {
                statusColor = goodColor;
            }
        } else if (id == "05" && rawInt >= 90) { // Make this configurable, currently hardcoded to 10%
            statusColor = cautionColor;
        } else {
            statusColor = goodColor;
        }

        QTableWidgetItem *statusItem = new QTableWidgetItem();
        statusItem->setData(Qt::BackgroundRole, QVariant(statusColor));

        QTableWidgetItem *idItem = new QTableWidgetItem(id);
        idItem->setTextAlignment(Qt::AlignCenter);

        QTableWidgetItem *nameItem = new QTableWidgetItem(name);

        QTableWidgetItem *rawItem = new QTableWidgetItem(raw);
        rawItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        tableWidget->setItem(row, 0, statusItem);
        tableWidget->setItem(row, 1, idItem);
        tableWidget->setItem(row, 2, nameItem);
        tableWidget->setItem(row, 3, rawItem);

        ++row;
    }

    tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    for (int i = 0; i < tableWidget->columnCount(); ++i) {
        if (i != 2) {
            tableWidget->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
        }
    }

    tableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    tableWidget->verticalHeader()->setDefaultSectionSize(31);
    tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

    if (!warningMessage.isEmpty()) {
        QMessageBox::warning(nullptr, tr("Critical Warning"), warningMessage);
    }
}

void MainWindow::addSmartAttributesTable(const QJsonArray &attributes)
{
    qsizetype rowCount = attributes.size();
    if (rowCount > std::numeric_limits<int>::max()) {
        rowCount = std::numeric_limits<int>::max();
    }

    tableWidget->setRowCount(static_cast<int>(rowCount));
    tableWidget->setColumnCount(7);
    tableWidget->setHorizontalHeaderLabels({"", tr("ID"), tr("Attribute Name"), tr("Current"), tr("Worst"), tr("Threshold"), tr("Raw Values")});
    tableWidget->verticalHeader()->setVisible(false);
    tableWidget->setItemDelegateForColumn(0, new StatusDot(tableWidget));

    for (int i = 0; i < tableWidget->columnCount(); ++i) {
        QTableWidgetItem *headerItem = tableWidget->horizontalHeaderItem(i);
        if (headerItem) {
            if (i == 2) {
                headerItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            } else if (i > 2) {
                headerItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            }
        }
    }

    int row = 0;
    for (const QJsonValue &attr : attributes) {
        QJsonObject attrObj = attr.toObject();
        QString id = QString("%1").arg(attrObj["id"].toInt(), 2, 16, QChar('0')).toUpper();
        QString name = attrObj["name"].toString().replace("_", " ");
        int value = attrObj["value"].toInt();
        int worst = attrObj["worst"].toInt();
        int thresh = attrObj["thresh"].toInt();
        QString rawString = attrObj["raw"].toObject()["string"].toString();
        QString rawHEX = rawString;

        qsizetype spaceIndex = rawHEX.indexOf(' ');
        if (spaceIndex != -1) {
            rawHEX = rawHEX.left(static_cast<int>(spaceIndex));
        }

        if (rawHEX.startsWith("0x") && rawHEX.length() == 14) {
            rawHEX = rawHEX.mid(2).toUpper();
        } else {
            rawHEX = QString("%1").arg(rawHEX.toUInt(nullptr), 12, 16, QChar('0')).toUpper();
        }

        QColor statusColor;
        if (thresh && (value < thresh)) {
            statusColor = badColor;
        } else if ((id == "05" || id == "C5" || id == "C6" || (id == "C4" && !(ui->actionIgnore_C4_Reallocation_Event_Count->isChecked()))) && (rawHEX != "000000000000")) {
            statusColor = cautionColor;
        } else {
            statusColor = goodColor;
        }

        QTableWidgetItem *statusItem = new QTableWidgetItem();
        statusItem->setData(Qt::BackgroundRole, QVariant(statusColor));

        QTableWidgetItem *idItem = new QTableWidgetItem(id);
        idItem->setTextAlignment(Qt::AlignCenter);

        QTableWidgetItem *nameItem = new QTableWidgetItem(name);

        QTableWidgetItem *valueItem = new QTableWidgetItem(QString::number(value));
        valueItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        QTableWidgetItem *worstItem = new QTableWidgetItem(QString::number(worst));
        worstItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        QTableWidgetItem *threshItem = new QTableWidgetItem(QString::number(thresh));
        threshItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        QTableWidgetItem *rawItem;
        if (ui->actionHEX->isChecked()) {
            rawItem = new QTableWidgetItem(rawHEX);
        } else {
            rawItem = new QTableWidgetItem(rawString);
        }
        rawItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        tableWidget->setItem(row, 0, statusItem);
        tableWidget->setItem(row, 1, idItem);
        tableWidget->setItem(row, 2, nameItem);
        tableWidget->setItem(row, 3, valueItem);
        tableWidget->setItem(row, 4, worstItem);
        tableWidget->setItem(row, 5, threshItem);
        tableWidget->setItem(row, 6, rawItem);

        ++row;
    }

    tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    for (int i = 0; i < tableWidget->columnCount(); ++i) {
        if (i != 2) {
            tableWidget->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
        }
    }

    tableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    tableWidget->verticalHeader()->setDefaultSectionSize(31);
    tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::ForwardButton && nextButton->isEnabled()) {
        onNextButtonClicked();
    } else if (event->button() == Qt::BackButton && prevButton->isEnabled()) {
        onPrevButtonClicked();
    }
}

// Slots
void MainWindow::on_actionQuit_triggered()
{
    qApp->quit();
}

void MainWindow::on_actionSave_JSON_triggered()
{
    if (deviceJson.isEmpty()) {
        QMessageBox::information(this, tr("Empty JSON"),
                                 tr("The JSON is empty"));
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this,
                                                    tr("Save JSON"), "QDiskInfo_" + deviceNodeLineEdit->text().section('/', -1) + ".json",
                                                    tr("JSON (*.json);;All Files (*)"));
    if (fileName.isEmpty())
        return;
    else {
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox::critical(this, tr("Unable to open file for writing"),
                                     file.errorString());
            return;
        }

        QJsonDocument doc(deviceJson);
        QByteArray jsonData = doc.toJson();

        file.write(jsonData);
        file.close();
    }
}

void MainWindow::on_actionGitHub_triggered()
{
    QDesktopServices::openUrl(QUrl("https://github.com/edisionnano/QDiskInfo"));
}

void MainWindow::on_actionAbout_QDiskInfo_triggered()
{
    QString message = tr("An ATA and NVMe S.M.A.R.T. data viewer for Linux") + "\n";
    message += tr("Licensed under the GNU G.P.L. Version 3") + "\n";
    message += tr("Made by Samantas5855") + "\n";
    message += tr("Version") + " " + QString::number(PROJECT_VERSION_MAJOR) + "." + QString::number(PROJECT_VERSION_MINOR);

    QMessageBox::about(this, tr("About QDiskInfo"), message);
}

void MainWindow::on_actionAbout_Qt_triggered()
{
    QMessageBox::aboutQt(this, tr("About Qt"));
}


void MainWindow::on_actionRescan_Refresh_triggered()
{
    QPair<QStringList, QJsonArray> values = Utils.scanDevices(initializing);
    deviceOutputs = values.first;
    devices = values.second;
    if (!deviceOutputs.isEmpty()) {
        updateUI(Utils.clearButtonGroup(buttonGroup, horizontalLayout, buttonStretch, menuDisk));
    }
}

void MainWindow::on_actionIgnore_C4_Reallocation_Event_Count_toggled(bool enabled)
{
    settings.setValue("IgnoreC4", enabled);
    if (!initializing) {
        updateUI(Utils.clearButtonGroup(buttonGroup, horizontalLayout, buttonStretch, menuDisk));
    }
}

void MainWindow::on_actionHEX_toggled(bool enabled)
{
    settings.setValue("HEX", enabled);
    if (!initializing) {
        updateUI(Utils.clearButtonGroup(buttonGroup, horizontalLayout, buttonStretch, menuDisk));
    }
}

void MainWindow::on_actionUse_Fahrenheit_toggled(bool enabled)
{
    settings.setValue("Fahrenheit", enabled);
    if (!initializing) {
        updateUI(Utils.clearButtonGroup(buttonGroup, horizontalLayout, buttonStretch, menuDisk));
    }
}

void MainWindow::on_actionCyclic_Navigation_toggled(bool cyclicNavigation)
{
    settings.setValue("CyclicNavigation", cyclicNavigation);
    qsizetype currentIndex = buttonGroup->buttons().indexOf(buttonGroup->checkedButton());
    updateNavigationButtons(currentIndex);
}

void MainWindow::on_actionUse_GB_instead_of_TB_toggled(bool gigabytes)
{
    settings.setValue("UseGB", gigabytes);
    if (!initializing) {
        updateUI(Utils.clearButtonGroup(buttonGroup, horizontalLayout, buttonStretch, menuDisk));
    }
}

void MainWindow::on_actionClear_Settings_triggered()
{
    QMessageBox msgBox;
    msgBox.setWindowTitle(QObject::tr("Clear Settings"));
    msgBox.setText(QObject::tr("Are you sure you want to clear the settings saved on disk?"));
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.addButton(QMessageBox::Cancel);
    QPushButton *clearSettingsButton = msgBox.addButton(QMessageBox::Ok);

    msgBox.exec();

    if (msgBox.clickedButton() == clearSettingsButton) {
        settings.clear();

        settings.setValue("IgnoreC4", true);
        settings.setValue("HEX", true);
        settings.setValue("Fahrenheit", false);
        settings.setValue("CyclicNavigation", false);
        settings.setValue("UseGB", false);

        ui->actionIgnore_C4_Reallocation_Event_Count->setChecked(true);
        ui->actionHEX->setChecked(true);
        ui->actionUse_Fahrenheit->setChecked(false);
        ui->actionCyclic_Navigation->setChecked(false);
        ui->actionUse_GB_instead_of_TB->setChecked(false);

        settings.setValue("Theme/Name", "__none__");
        setThemeSelection(QStringLiteral("__none__"), false);
        rebuildThemeMenu();

        if (!initializing) {
            updateUI(Utils.clearButtonGroup(buttonGroup, horizontalLayout, buttonStretch, menuDisk));
        }
    }
}

void MainWindow::on_actionASCII_View_triggered()
{
    QString deviceNodePath = deviceNodeLineEdit->text();

    QProcess *process = new QProcess(this);
    process->start("pkexec", {QCoreApplication::applicationFilePath(), "--ascii-view", deviceNodePath});

    if (!process->waitForFinished()) {
        return;
    }

    QByteArray binaryData = process->readAllStandardOutput();
    QByteArray errorData = process->readAllStandardError();

    if (!errorData.isEmpty()) {
        QMessageBox::critical(this, tr("QDiskInfo Error"), tr("QDiskInfo needs root access in order to read S.M.A.R.T. data!"));
        return;
    }

    AsciiView asciiview;
    QString hexDumpOutput = asciiview.hexDump(QVector<unsigned char>(binaryData.begin(), binaryData.end()));

    QDialog *asciiViewDialog = new QDialog(this);
    asciiViewDialog->setWindowTitle(tr("ASCII View"));

    QTextEdit *textView = new QTextEdit(asciiViewDialog);
    textView->setText(hexDumpOutput);
    textView->setReadOnly(true);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Close, asciiViewDialog);
    connect(buttonBox, &QDialogButtonBox::rejected, asciiViewDialog, &QDialog::close);
    connect(buttonBox->button(QDialogButtonBox::Save), &QPushButton::clicked, this, [binaryData, this, deviceNodePath]() {
        QString filePath = QFileDialog::getSaveFileName(this, tr("Save Binary Data"),  deviceNodePath.section('/', -1) + ".bin", tr("Binary Files (*.bin);;All Files (*)"));
        if (!filePath.isEmpty()) {
            QFile file(filePath);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(binaryData);
                file.close();
                QMessageBox::information(this, tr("Success"), tr("Binary data saved successfully."));
            } else {
                QMessageBox::critical(this, tr("Unable to open file for writing"), file.errorString());
            }
        }
    });

    QVBoxLayout *layout = new QVBoxLayout(asciiViewDialog);
    layout->addWidget(textView);
    layout->addWidget(buttonBox);

    asciiViewDialog->setLayout(layout);
    asciiViewDialog->resize(550, 500);
    asciiViewDialog->exec();
}

void MainWindow::on_actionGrid_View_triggered()
{
    gridView->show();
}


void MainWindow::on_actionSave_Image_triggered()
{
    QPixmap screenshot = this->grab();
    screenshot.save(QFileDialog::getSaveFileName(this, tr("Save Image"), "QDiskInfo_" + deviceNodeLineEdit->text().section('/', -1) + ".png", tr("PNG Files (*.png)")));
}

QString MainWindow::getMbToPrettyString(const int64_t &sizeInMbI64, const int &precisionInt, const bool &useGbBool) const
{
    if (!sizeInMbI64)
        return "----";

    int precision = precisionInt ? (useGbBool ? 1 : precisionInt) : 0;
    double sizeInMB = double(sizeInMbI64);
    if (sizeInMbI64 < 1000)
        return QString::number(sizeInMbI64) + " " + mbSymbol;
    else if (sizeInMbI64 < 1000000 || useGbBool)
        return QString::number(sizeInMB/1e3, 'f', precision) + " " + gbSymbol;
    else if (sizeInMbI64 < 1000000000)
        return QString::number(sizeInMB/1e6, 'f', precision) + " " + tbSymbol;
    return QString::number(sizeInMB/1e9, 'f', precision) + " " + pbSymbol;
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (themedMode) {
        themeBackgroundTimer.start();
    }
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (!screenChangeConnected && windowHandle()) {
        screenChangeConnected = true;
        connect(windowHandle(), &QWindow::screenChanged, this, [this](QScreen *) {
            if (!themedMode) {
                return;
            }
            if (!builtinThemeActive) {
                const QString bgPath = currentTheme.backgroundPath(themeZoomPercent());
                if (!bgPath.isEmpty()) {
                    themeBackgroundSource = QPixmap(bgPath);
                } else if (currentTheme.backgroundColor().isValid()) {
                    QPixmap colorPixmap(1, 1);
                    colorPixmap.fill(currentTheme.backgroundColor());
                    themeBackgroundSource = colorPixmap;
                } else {
                    themeBackgroundSource = QPixmap();
                }
            }
            updateThemeBackground();
            updateStatusIllustration(globalHealth);
        });
    }
    if (themedMode) {
        updateThemeBackground();
        updateStatusIllustration(globalHealth);
    }
}

void MainWindow::setupThemeMenu()
{
    themeMenu = ui->menuSettings->addMenu(tr("Theme"));
    connect(themeMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildThemeMenu);
    rebuildThemeMenu();
}

void MainWindow::rebuildThemeMenu()
{
    if (!themeMenu) {
        return;
    }

    if (themeActionGroup) {
        delete themeActionGroup;
        themeActionGroup = nullptr;
    }

    themeMenu->clear();
    themeActionGroup = new QActionGroup(this);
    themeActionGroup->setExclusive(true);

    QAction *defaultAction = themeMenu->addAction(tr("Default (No Theme)"));
    defaultAction->setCheckable(true);
    defaultAction->setData(QStringLiteral("__none__"));
    themeActionGroup->addAction(defaultAction);
    connect(defaultAction, &QAction::triggered, this, [this]() {
        setThemeSelection(QStringLiteral("__none__"), true);
    });

#if INCLUDE_OPTIONAL_RESOURCES
    QAction *builtinAction = themeMenu->addAction(tr("Built-in (compile-time)"));
    builtinAction->setCheckable(true);
    builtinAction->setData(QStringLiteral("__builtin__"));
    themeActionGroup->addAction(builtinAction);
    connect(builtinAction, &QAction::triggered, this, [this]() {
        setThemeSelection(QStringLiteral("__builtin__"), true);
    });
#endif

    const QStringList names = themeManager.themeNames();
    for (const QString &tName : names) {
        QAction *themeAction = themeMenu->addAction(tName);
        themeAction->setCheckable(true);
        themeAction->setData(tName);
        themeActionGroup->addAction(themeAction);
        connect(themeAction, &QAction::triggered, this, [this, tName]() {
            setThemeSelection(tName, true);
        });
    }

    themeMenu->addSeparator();

    QAction *importAction = themeMenu->addAction(tr("Import Themes from CDI ZIP..."));
    connect(importAction, &QAction::triggered, this, &MainWindow::importThemesFromZip);

    QAction *openFolderAction = themeMenu->addAction(tr("Open Themes Folder"));
    connect(openFolderAction, &QAction::triggered, this, &MainWindow::openThemesFolder);

    for (QAction *act : themeActionGroup->actions()) {
        if (act->data().toString() == currentThemeName) {
            act->setChecked(true);
            break;
        }
    }
}

void MainWindow::setThemeSelection(const QString &name, bool persist)
{
    if (name.isEmpty() || name == QLatin1String("__none__")) {
        clearThemeVisuals();
        currentThemeName = QStringLiteral("__none__");
    } else if (name == QLatin1String("__builtin__")) {
#if INCLUDE_OPTIONAL_RESOURCES
        applyBuiltinTheme();
        currentThemeName = QStringLiteral("__builtin__");
#else
        clearThemeVisuals();
        currentThemeName = QStringLiteral("__none__");
#endif
    } else {
        const Theme *t = themeManager.theme(name);
        if (t && t->isValid()) {
            applyThemeVisuals(*t);
            currentThemeName = name;
        } else {
            clearThemeVisuals();
            currentThemeName = QStringLiteral("__none__");
        }
    }

    if (persist) {
        settings.setValue(QStringLiteral("Theme/Name"), currentThemeName);
    }

    if (themeActionGroup) {
        const QList<QAction *> actions = themeActionGroup->actions();
        for (QAction *act : actions) {
            if (act->data().toString() == currentThemeName) {
                act->setChecked(true);
                break;
            }
        }
    }

    refreshCurrentDevice();
}

void MainWindow::importThemesFromZip()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Import CDI Themes"),
        QDir::homePath(),
        tr("ZIP archives (*.zip)"));

    if (filePath.isEmpty()) {
        return;
    }

    const ThemeZipInfo info = themeManager.inspectZip(filePath);
    if (!info.ok()) {
        QMessageBox::critical(this, tr("Import Error"), info.error);
        return;
    }

    bool overwrite = false;
    if (!info.existing.isEmpty()) {
        const QString prompt = tr("The following themes already exist:\n\n%1\n\nDo you want to overwrite them?")
            .arg(info.existing.join(QLatin1Char('\n')));
        const QMessageBox::StandardButton btn = QMessageBox::question(
            this,
            tr("Overwrite Themes"),
            prompt,
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        overwrite = (btn == QMessageBox::Yes);
    }

    const ThemeImportResult result = themeManager.importZip(filePath, overwrite);
    if (!result.ok()) {
        QMessageBox::critical(this, tr("Import Error"), result.error);
        return;
    }

    themeManager.reload();
    QTimer::singleShot(0, this, &MainWindow::rebuildThemeMenu);

    if (currentThemeName != QLatin1String("__none__")) {
        const Theme *t = themeManager.theme(currentThemeName);
        if (t && t->isValid()) {
            applyThemeVisuals(*t);
        } else {
            clearThemeVisuals();
            currentThemeName = QStringLiteral("__none__");
            settings.setValue(QStringLiteral("Theme/Name"), currentThemeName);
        }
    } else {
        for (const QString &importedName : result.imported) {
            const Theme *t = themeManager.theme(importedName);
            if (t && t->hasOwnBackground()) {
                setThemeSelection(importedName, true);
                break;
            }
        }
    }

    QString summary = tr("Imported: %1 themes.").arg(result.imported.size());
    if (!result.skipped.isEmpty()) {
        summary += tr("\nSkipped: %1 themes.").arg(result.skipped.size());
    }
    if (!result.failed.isEmpty()) {
        summary += tr("\nFailed: %1 themes.").arg(result.failed.size());
    }
    if (!result.duplicates.isEmpty()) {
        summary += tr("\nDuplicate theme folders ignored: %1").arg(result.duplicates.join(QStringLiteral(", ")));
    }
    QMessageBox::information(this, tr("Import Complete"), summary);
}

void MainWindow::openThemesFolder()
{
    const QString root = themeManager.writableRoot();
    QDir().mkpath(root);
    QDesktopServices::openUrl(QUrl::fromLocalFile(root));
}

void MainWindow::applyThemeVisuals(const Theme &theme)
{
    currentTheme = theme;
    themedMode = true;
    builtinThemeActive = false;

    applyThemeLayout(true, theme.characterPosition() == 1);

    if (statusLabel) {
        statusLabel->hide();
    }

    const QString bgPath = theme.backgroundPath(themeZoomPercent());
    if (!bgPath.isEmpty()) {
        themeBackgroundSource = QPixmap(bgPath);
    } else if (theme.backgroundColor().isValid()) {
        QPixmap px(1, 1);
        px.fill(theme.backgroundColor());
        themeBackgroundSource = px;
    } else {
        themeBackgroundSource = QPixmap();
    }

    if (themeBackgroundSource.isNull()) {
        ui->centralwidget->setPalette(defaultCentralPalette);
        ui->centralwidget->setAutoFillBackground(false);
    }

    applyThemeColors(theme.colors());
    updateThemeBackground();
    refreshCurrentDevice();
}

void MainWindow::applyBuiltinTheme()
{
#if INCLUDE_OPTIONAL_RESOURCES
    applyThemeLayout(true, CHARACTER_IS_RIGHT != 0);
    themedMode = true;
    builtinThemeActive = true;

    const QColor backgroundColor = QApplication::palette().color(QPalette::Window);
    const bool darkTheme = backgroundColor.lightness() < 128;
    themeBackgroundSource = QPixmap(darkTheme ? QStringLiteral(":/icons/bg_dark.png")
                                              : QStringLiteral(":/icons/bg_light.png"));

    restoreDefaultColors();

    setMaximumSize(960, 960);
    resize(960, 600);

    updateThemeBackground();
    updateStatusIllustration(globalHealth);
    refreshCurrentDevice();
#else
    clearThemeVisuals();
    currentThemeName = QStringLiteral("__none__");
#endif
}

void MainWindow::clearThemeVisuals()
{
    themedMode = false;
    builtinThemeActive = false;
    themeBackgroundSource = QPixmap();
    themeBackgroundTimer.stop();

    applyThemeLayout(false, false);

    restoreDefaultColors();
    ui->centralwidget->setPalette(defaultCentralPalette);
    ui->centralwidget->setAutoFillBackground(false);
    refreshCurrentDevice();
}

void MainWindow::applyThemeColors(const ThemeColors &colors)
{
    QWidget *cw = ui->centralwidget;

    QPalette centralPal = defaultCentralPalette;
    centralPal.setColor(QPalette::WindowText, colors.labelText);
    centralPal.setColor(QPalette::Text, colors.labelText);
    centralPal.setColor(QPalette::ButtonText, colors.buttonText);
    cw->setPalette(centralPal);

    QColor base1 = colors.listBg1;
    base1.setAlpha(colors.glassAlpha);
    QColor base2 = colors.listBg2;
    base2.setAlpha(colors.glassAlpha);

    QColor highlight = colors.glass;
    highlight.setAlpha(220);

    const double lum = 0.299 * colors.glass.red() + 0.587 * colors.glass.green() + 0.114 * colors.glass.blue();
    const QColor highlightedText = (lum > 128.0) ? QColor(Qt::black) : QColor(Qt::white);

    QPalette tablePal = defaultTablePalette;
    tablePal.setColor(QPalette::Base, base1);
    tablePal.setColor(QPalette::AlternateBase, base2);
    tablePal.setColor(QPalette::Text, colors.listText1);
    tablePal.setColor(QPalette::Highlight, highlight);
    tablePal.setColor(QPalette::HighlightedText, highlightedText);
    tableWidget->setPalette(tablePal);

    tableWidget->setAlternatingRowColors(true);
    tableWidget->viewport()->setAutoFillBackground(false);

    cw->setStyleSheet(buildThemeStyleSheet(colors));

    updateIconFallbacks();
}

void MainWindow::restoreDefaultColors()
{
    QWidget *cw = ui->centralwidget;
    cw->setStyleSheet(QString());
    cw->setPalette(defaultCentralPalette);
    tableWidget->setPalette(defaultTablePalette);
    tableWidget->setAlternatingRowColors(defaultAlternatingRowColors);
    tableWidget->viewport()->setAutoFillBackground(defaultViewportAutoFillBackground);

    updateIconFallbacks();
}

void MainWindow::updateThemeBackground()
{
    if (themeBackgroundSource.isNull()) {
        return;
    }

    QWidget *cw = ui->centralwidget;
    const QSize targetSize = cw->size();
    if (targetSize.isEmpty()) {
        return;
    }

    const qreal dpr = cw->devicePixelRatioF();
    const QSize targetPx = targetSize * dpr;
    if (targetPx.isEmpty()) {
        return;
    }

    const QPixmap scaled = themeBackgroundSource.scaled(
        targetPx,
        Qt::KeepAspectRatioByExpanding,
        Qt::SmoothTransformation);

    QPixmap canvas(targetPx);
    canvas.fill(Qt::transparent);

    {
        QPainter painter(&canvas);
        const bool characterRight = builtinThemeActive
            ? (CHARACTER_IS_RIGHT != 0)
            : (currentTheme.characterPosition() == 1);
        const int x = characterRight ? (targetPx.width() - scaled.width()) : 0;
        const int y = 0;
        painter.drawPixmap(x, y, scaled);
    }

    canvas.setDevicePixelRatio(dpr);

    QPalette pal = cw->palette();
    pal.setBrush(QPalette::Window, QBrush(canvas));
    cw->setPalette(pal);
    cw->setAutoFillBackground(true);
}

int MainWindow::themeZoomPercent() const
{
    QScreen *s = screen();
    if (!s) {
        s = QGuiApplication::primaryScreen();
    }
    const qreal dpr = s ? s->devicePixelRatio() : 1.0;
    return qBound(100, qRound(dpr * 100.0), 300);
}

void MainWindow::initializeDefaultThemeState()
{
    themeLayoutActive = false;
    if (temperatureLabelHorizontal) temperatureLabelHorizontal->hide();
    if (healthStatusLabelHorizontal) healthStatusLabelHorizontal->hide();
    if (temperatureValueHorizontal) temperatureValueHorizontal->hide();
    if (healthStatusValueHorizontal) healthStatusValueHorizontal->hide();

    if (temperatureLabelVertical) temperatureLabelVertical->show();
    if (healthStatusLabelVertical) healthStatusLabelVertical->show();
    if (temperatureValue) temperatureValue->show();
    if (healthStatusValue) healthStatusValue->show();

    if (statusLabel) statusLabel->hide();

    if (gridLeftSpacer && gridLayout) {
        gridLayout->removeItem(gridLeftSpacer);
    }
}

void MainWindow::applyThemeLayout(bool enabled, bool characterRight)
{
    if (enabled == themeLayoutActive) {
        if (!enabled) {
            return;
        }
        if (characterRight) {
            if (gridLeftSpacer && gridLayout && gridLayout->indexOf(gridLeftSpacer) >= 0) {
                gridLayout->removeItem(gridLeftSpacer);
            }
            if (!characterSpacer && gridLayout) {
                characterSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
                gridLayout->addItem(characterSpacer, 3, 3);
            }
            setMinimumWidth(645);
        } else {
            if (characterSpacer && gridLayout) {
                gridLayout->removeItem(characterSpacer);
                delete characterSpacer;
                characterSpacer = nullptr;
            }
            if (gridLeftSpacer && gridLayout && gridLayout->indexOf(gridLeftSpacer) < 0) {
                gridLayout->addItem(gridLeftSpacer, 2, 0);
            }
            setMinimumWidth(960);
        }
        setMaximumSize(defaultMaximumSize);
        return;
    }

    if (enabled) {
        if (healthStatusLabelVertical) healthStatusLabelVertical->hide();
        if (temperatureLabelVertical) temperatureLabelVertical->hide();
        if (healthStatusValue) healthStatusValue->hide();
        if (temperatureValue) temperatureValue->hide();

        if (verticalLayout) {
            for (const QPair<int, QSpacerItem *> &entry : std::as_const(verticalSpacers)) {
                if (verticalLayout->indexOf(entry.second) >= 0) {
                    verticalLayout->removeItem(entry.second);
                }
            }
            if (statusLabel && verticalLayout->indexOf(statusLabel) < 0) {
                verticalLayout->addWidget(statusLabel);
            }
        }
        if (statusLabel) {
            statusLabel->show();
        }

        if (temperatureLabelHorizontal) temperatureLabelHorizontal->show();
        if (healthStatusLabelHorizontal) healthStatusLabelHorizontal->show();
        if (temperatureValueHorizontal) temperatureValueHorizontal->show();
        if (healthStatusValueHorizontal) healthStatusValueHorizontal->show();

        if (infoSpacer && infoLayout && infoLayout->indexOf(infoSpacer) >= 0) {
            infoLayout->removeItem(infoSpacer);
        }

        if (characterRight) {
            if (gridLeftSpacer && gridLayout && gridLayout->indexOf(gridLeftSpacer) >= 0) {
                gridLayout->removeItem(gridLeftSpacer);
            }
            if (!characterSpacer && gridLayout) {
                characterSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
                gridLayout->addItem(characterSpacer, 3, 3);
            }
            setMinimumWidth(645);
        } else {
            if (characterSpacer && gridLayout) {
                gridLayout->removeItem(characterSpacer);
                delete characterSpacer;
                characterSpacer = nullptr;
            }
            if (gridLeftSpacer && gridLayout && gridLayout->indexOf(gridLeftSpacer) < 0) {
                gridLayout->addItem(gridLeftSpacer, 2, 0);
            }
            setMinimumWidth(960);
        }

        setMaximumSize(defaultMaximumSize);
        if (width() < 960 || height() < 600) {
            resize(qMax(width(), 960), qMax(height(), 600));
        }
    } else {
        if (verticalLayout && statusLabel && verticalLayout->indexOf(statusLabel) >= 0) {
            verticalLayout->removeWidget(statusLabel);
        }
        if (statusLabel) {
            statusLabel->hide();
        }

        if (healthStatusLabelVertical) healthStatusLabelVertical->show();
        if (temperatureLabelVertical) temperatureLabelVertical->show();
        if (healthStatusValue) healthStatusValue->show();
        if (temperatureValue) temperatureValue->show();

        if (verticalLayout) {
            QVector<QPair<int, QSpacerItem *>> spacers = verticalSpacers;
            std::sort(spacers.begin(), spacers.end(),
                      [](const QPair<int, QSpacerItem *> &a, const QPair<int, QSpacerItem *> &b) {
                          return a.first < b.first;
                      });
            for (const QPair<int, QSpacerItem *> &entry : std::as_const(spacers)) {
                if (verticalLayout->indexOf(entry.second) < 0) {
                    verticalLayout->insertSpacerItem(entry.first, entry.second);
                }
            }
        }

        if (temperatureLabelHorizontal) temperatureLabelHorizontal->hide();
        if (healthStatusLabelHorizontal) healthStatusLabelHorizontal->hide();
        if (temperatureValueHorizontal) temperatureValueHorizontal->hide();
        if (healthStatusValueHorizontal) healthStatusValueHorizontal->hide();

        if (infoSpacer && infoLayout && infoLayout->indexOf(infoSpacer) < 0) {
            infoLayout->insertSpacerItem(1, infoSpacer);
        }

        if (characterSpacer && gridLayout) {
            gridLayout->removeItem(characterSpacer);
            delete characterSpacer;
            characterSpacer = nullptr;
        }
        if (gridLeftSpacer && gridLayout && gridLayout->indexOf(gridLeftSpacer) >= 0) {
            gridLayout->removeItem(gridLeftSpacer);
        }

        setMinimumWidth(defaultMinimumWidth);
        setMaximumSize(defaultMaximumSize);
    }
    themeLayoutActive = enabled;
}

void MainWindow::updateStatusIllustration(const QString &health)
{
    if (!statusLabel) {
        return;
    }
    if (!themeLayoutActive || !builtinThemeActive) {
        // Only the built-in (compile-time) theme draws a health illustration in the
        // status slot; runtime themes leave the slot hidden.
        statusLabel->hide();
        return;
    }

    QPixmap pixmap;
    if (health == tr("Bad")) {
        pixmap = QPixmap(":/icons/bad.png");
    } else if (health == tr("Caution")) {
        pixmap = QPixmap(":/icons/caution.png");
    } else if (health == tr("Good")) {
        pixmap = QPixmap(":/icons/good.png");
    } else {
        pixmap = QPixmap(":/icons/unknown.png");
    }
    const QPixmap scaled = pixmap.scaled(QSize(100, 30), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    statusLabel->setPixmap(scaled);
    statusLabel->setFixedSize(scaled.size() / scaled.devicePixelRatio());
    statusLabel->setToolTip(health);
    statusLabel->show();
}

void MainWindow::refreshCurrentDevice()
{
    if (!deviceJson.isEmpty()) {
        populateWindow(deviceJson, globalHealth, globalIsNvme ? globalNvmeSmartOrdered : QVector<QPair<QString, int>>());
    }
}

QString MainWindow::themeButtonTextColor() const
{
    if (themedMode && !builtinThemeActive) {
        return currentTheme.colors().buttonText.name();
    }
    return QStringLiteral("black");
}

QColor MainWindow::iconForegroundColor() const
{
    if (themedMode && !builtinThemeActive) {
        return currentTheme.colors().labelText;
    }
    return palette().color(QPalette::ButtonText);
}

void MainWindow::updateIconFallbacks()
{
    const QColor color = iconForegroundColor();

    if (prevButton) {
        prevButton->setIcon(themedIconOrFallback(QStringLiteral("go-previous"), color));
    }
    if (nextButton) {
        nextButton->setIcon(themedIconOrFallback(QStringLiteral("go-next"), color));
    }
    if (serialEchoAction && serialNumberLineEdit) {
        const bool hidden = serialNumberLineEdit->echoMode() == QLineEdit::Password;
        serialEchoAction->setIcon(
            themedIconOrFallback(hidden ? QStringLiteral("visibility") : QStringLiteral("hint"), color));
    }
}
