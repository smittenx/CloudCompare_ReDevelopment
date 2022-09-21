//##########################################################################
//#                                                                        #
//#                              CLOUDCOMPARE                              #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU General Public License as published by  #
//#  the Free Software Foundation; version 2 or later of the License.      #
//#                                                                        #
//#  This program is distributed in the hope that it will be useful,       #
//#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          #
//#  GNU General Public License for more details.                          #
//#                                                                        #
//#          COPYRIGHT: EDF R&D / TELECOM ParisTech (ENST-TSI)             #
//#                                                                        #
//##########################################################################


#include "ccPointListPickingDlg.h"

//Qt
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QtWidgets/QTableWidgetItem>
#include <QInputDialog>
#include <QByteArray>
#include <QComboBox>
#include <QStringList>

//CCCoreLib
#include <CCConst.h>

//qCC_db
#include <cc2DLabel.h>
#include <ccLog.h>
#include <ccPointCloud.h>
#include <ccPolyline.h>

//qCC_io
#include <AsciiFilter.h>

//local
#include "ccGLWindow.h"
#include "mainwindow.h"

#include "db_tree/ccDBRoot.h"

//system
#include <cassert>

#include <cstring>

class QTableWidgetItem;
//class QComboBox;

//semi persistent settings
static int s_pickedPointsStartIndex = 0;
static bool s_showGlobalCoordsCheckBoxChecked = false;
static const char s_pickedPointContainerName[] = "Picked points list";
static const char s_defaultLabelBaseName[] = "Point #";
//static char s_defaultLabelBaseName[] = "Point #";
ccPointListPickingDlg::ccPointListPickingDlg(ccPickingHub* pickingHub, QWidget* parent)
    : ccPointPickingGenericInterface(pickingHub, parent)
    , Ui::PointListPickingDlg()
    , m_associatedEntity(nullptr)
    , m_lastPreviousID(0)
    , m_orderedLabelsContainer(nullptr)
{
    setupUi(this);

	exportToolButton->setPopupMode(QToolButton::MenuButtonPopup);
	QMenu* menu = new QMenu(exportToolButton);
	/*
	QAction* exportASCII_xyz = menu->addAction("x,y,z");
	QAction* exportASCII_ixyz = menu->addAction("local index,x,y,z");
	QAction* exportASCII_gxyz = menu->addAction("global index,x,y,z");
	QAction* exportASCII_lxyz = menu->addAction("label name,x,y,z");
    */
    QAction* exportASCII_Icxyz = menu->addAction("label name, class, x, y, z");
    /*
	QAction* exportToNewCloud = menu->addAction("new cloud");
	QAction* exportToNewPolyline = menu->addAction("new polyline");
	*/
    QAction *exportToReferencePolyline = menu->addAction("new reference polyline");

	exportToolButton->setMenu(menu);

	tableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

	startIndexSpinBox->setValue(s_pickedPointsStartIndex);
	showGlobalCoordsCheckBox->setChecked(s_showGlobalCoordsCheckBoxChecked);

	connect(cancelToolButton,		&QAbstractButton::clicked,	this,				&ccPointListPickingDlg::cancelAndExit);
	connect(revertToolButton,		&QAbstractButton::clicked,	this,				&ccPointListPickingDlg::removeLastEntry);
	connect(validToolButton,		&QAbstractButton::clicked,	this,				&ccPointListPickingDlg::applyAndExit);
	connect(exportToolButton,		&QAbstractButton::clicked,	exportToolButton,	&QToolButton::showMenu);
	/*
	connect(exportASCII_xyz,		&QAction::triggered,		this,				&ccPointListPickingDlg::exportToASCII_xyz);
	connect(exportASCII_ixyz,		&QAction::triggered,		this,				&ccPointListPickingDlg::exportToASCII_ixyz);
	connect(exportASCII_gxyz,		&QAction::triggered,		this,				&ccPointListPickingDlg::exportToASCII_gxyz);
	connect(exportASCII_lxyz,		&QAction::triggered,		this,				&ccPointListPickingDlg::exportToASCII_lxyz);
	*/
	connect(exportASCII_Icxyz,       &QAction::triggered,      this,        &ccPointListPickingDlg::exportToASCII_Icxyz);
	/*
	connect(exportToNewCloud,		&QAction::triggered,		this,				&ccPointListPickingDlg::exportToNewCloud);
	connect(exportToNewPolyline,	&QAction::triggered,		this,				&ccPointListPickingDlg::exportToNewPolyline);
	*/
    connect(exportToReferencePolyline, &QAction::triggered, this, &ccPointListPickingDlg::exportToReferencePolyline);

	connect(setLabel,                              &QAbstractButton::clicked,         this,              &ccPointListPickingDlg::setlabel);
	
	connect(markerSizeSpinBox,	qOverload<int>(&QSpinBox::valueChanged),	this,	&ccPointListPickingDlg::markerSizeChanged);
	connect(startIndexSpinBox,	qOverload<int>(&QSpinBox::valueChanged),	this,	&ccPointListPickingDlg::startIndexChanged);
	
	connect(showGlobalCoordsCheckBox, &QAbstractButton::clicked, this, &ccPointListPickingDlg::updateList);

	updateList();
}

unsigned ccPointListPickingDlg::getPickedPoints(std::vector<cc2DLabel*>& pickedPoints)
{
	pickedPoints.clear();

	if (m_orderedLabelsContainer)
	{
		//get all labels
		ccHObject::Container labels;
		unsigned count = m_orderedLabelsContainer->filterChildren(labels, false, CC_TYPES::LABEL_2D);

		try
		{
			pickedPoints.reserve(count);
		}
		catch (const std::bad_alloc&)
		{
			ccLog::Error("Not enough memory!");
			return 0;
		}
		for (unsigned i = 0; i < count; ++i)
		{
			if (labels[i]->isA(CC_TYPES::LABEL_2D)) //Warning: cc2DViewportLabel is also a kind of 'CC_TYPES::LABEL_2D'!
			{
				cc2DLabel* label = static_cast<cc2DLabel*>(labels[i]);
				if (label->isVisible() && label->size() == 1)
				{
					pickedPoints.push_back(label);
				}
			}
		}
	}

	return static_cast<unsigned>(pickedPoints.size());
}

void ccPointListPickingDlg::linkWithEntity(ccHObject* entity)
{
	m_associatedEntity = entity;
	m_lastPreviousID = 0;

	if (m_associatedEntity)
	{
		//find default container
		m_orderedLabelsContainer = nullptr;
		ccHObject::Container groups;
		m_associatedEntity->filterChildren(groups, true, CC_TYPES::HIERARCHY_OBJECT);

		for (ccHObject::Container::const_iterator it = groups.begin(); it != groups.end(); ++it)
		{
			if ((*it)->getName() == s_pickedPointContainerName)
			{
				m_orderedLabelsContainer = *it;
				break;
			}
		}

		std::vector<cc2DLabel*> previousPickedPoints;
		unsigned count = getPickedPoints(previousPickedPoints);
		//find highest unique ID among the VISIBLE labels
		for (unsigned i = 0; i < count; ++i)
		{
			m_lastPreviousID = std::max(m_lastPreviousID, previousPickedPoints[i]->getUniqueID());
		}
	}

	ccShiftedObject* shifted = ccHObjectCaster::ToShifted(entity);
	showGlobalCoordsCheckBox->setEnabled(shifted ? shifted->isShifted() : false);
	updateList();
}

void ccPointListPickingDlg::cancelAndExit()
{
	ccDBRoot* dbRoot = MainWindow::TheInstance()->db();
	if (!dbRoot)
	{
		assert(false);
		return;
	}

	if (m_orderedLabelsContainer)
	{
		//Restore previous state
		if (!m_toBeAdded.empty())
		{
			dbRoot->removeElements(m_toBeAdded);
		}

		for (auto & object : m_toBeDeleted)
		{
			object->prepareDisplayForRefresh();
			object->setEnabled(true);
		}

		if (m_orderedLabelsContainer->getChildrenNumber() == 0)
		{
			dbRoot->removeElement(m_orderedLabelsContainer);
			//m_associatedEntity->removeChild(m_orderedLabelsContainer);
			m_orderedLabelsContainer = nullptr;
		}
	}

	m_toBeDeleted.resize(0);
	m_toBeAdded.resize(0);
	m_associatedEntity = nullptr;
	m_orderedLabelsContainer = nullptr;

	MainWindow::RefreshAllGLWindow();

	updateList();

	stop(false);
}

void ccPointListPickingDlg::exportToNewCloud()
{
	if (!m_associatedEntity)
		return;

	//get all labels
	std::vector<cc2DLabel*> labels;
	unsigned count = getPickedPoints(labels);
	if (count != 0)
	{
		ccPointCloud* cloud = new ccPointCloud();
		if (cloud->reserve(count))
		{
			cloud->setName("Picking list");
			for (unsigned i = 0; i < count; ++i)
			{
				const cc2DLabel::PickedPoint& PP = labels[i]->getPickedPoint(0);
				cloud->addPoint(PP.getPointPosition());
			}

			cloud->setDisplay(m_associatedEntity->getDisplay());

			//retrieve Shift & Scale values
			ccShiftedObject* shifted = ccHObjectCaster::ToShifted(m_associatedEntity);
			if (shifted)
			{
				cloud->copyGlobalShiftAndScale(*shifted);
			}
			else
			{
				assert(false);
			}
			MainWindow::TheInstance()->addToDB(cloud);
		}
		else
		{
			ccLog::Error("Can't export picked points as point cloud: not enough memory!");
			delete cloud;
			cloud = nullptr;
		}
	}
	else
	{
		ccLog::Error("Pick some points first!");
	}
}

void ccPointListPickingDlg::exportToNewPolyline()
{
	if (!m_associatedEntity)
		return;

	//get all labels
	std::vector<cc2DLabel*> labels;
	unsigned count = getPickedPoints(labels);
	if (count > 1)
	{
		//we create an "independent" polyline
		ccPointCloud* vertices = new ccPointCloud("vertices");
		ccPolyline* polyline = new ccPolyline(vertices);

		if (!vertices->reserve(count) || !polyline->reserve(count))
		{
			ccLog::Error("Not enough memory!");
			delete vertices;
			delete polyline;
			return;
		}

		for (unsigned i = 0; i < count; ++i)
		{
			const cc2DLabel::PickedPoint& PP = labels[i]->getPickedPoint(0);
			vertices->addPoint(PP.getPointPosition());
		}
		polyline->addPointIndex(0, count);
		polyline->setVisible(true);
		vertices->setEnabled(false);
		//retrieve Shift & Scale values
		ccGenericPointCloud* asCloud = ccHObjectCaster::ToGenericPointCloud(m_associatedEntity);
		if (asCloud)
		{
			polyline->setGlobalShift(asCloud->getGlobalShift());
			polyline->setGlobalScale(asCloud->getGlobalScale());
		}
		else
		{
			assert(false);
		}
		polyline->addChild(vertices);
		polyline->setDisplay_recursive(m_associatedEntity->getDisplay());

		MainWindow::TheInstance()->addToDB(polyline);
	}
	else
	{
		ccLog::Error("Pick at least two points!");
	}
}

void ccPointListPickingDlg::applyAndExit()
{
	if (m_associatedEntity && !m_toBeDeleted.empty())
	{
		//apply modifications
		MainWindow::TheInstance()->db()->removeElements(m_toBeDeleted); //no need to redraw as they should already be invisible
		m_associatedEntity = nullptr;
	}

	m_toBeDeleted.resize(0);
	m_toBeAdded.resize(0);
	m_orderedLabelsContainer = nullptr;

	updateList();

	stop(true);
}

void ccPointListPickingDlg::removeLastEntry()
{
	if (!m_associatedEntity)
		return;

	//get all labels
	std::vector<cc2DLabel*> labels;
	unsigned count = getPickedPoints(labels);
	if (count == 0)
		return;

	ccHObject* lastVisibleLabel = labels.back();
	if (lastVisibleLabel->getUniqueID() <= m_lastPreviousID)
	{
		//old label: hide it and add it to the 'to be deleted' list (will be restored if process is cancelled)
		lastVisibleLabel->setEnabled(false);
		m_toBeDeleted.push_back(lastVisibleLabel);
	}
	else
	{
		if (!m_toBeAdded.empty())
		{
			assert(m_toBeAdded.back() == lastVisibleLabel);
			m_toBeAdded.pop_back();
		}

		if (m_orderedLabelsContainer)
		{
			if (lastVisibleLabel->getParent())
			{
				lastVisibleLabel->getParent()->removeDependencyWith(lastVisibleLabel);
				lastVisibleLabel->removeDependencyWith(lastVisibleLabel->getParent());
			}
			//m_orderedLabelsContainer->removeChild(lastVisibleLabel);
			MainWindow::TheInstance()->db()->removeElement(lastVisibleLabel);
		}
		else
			m_associatedEntity->detachChild(lastVisibleLabel);
	}

	updateList();

	if (m_associatedWin)
		m_associatedWin->redraw();
}

void ccPointListPickingDlg::startIndexChanged(int value)
{
	if (value != s_pickedPointsStartIndex)
	{
		s_pickedPointsStartIndex = value;

		updateList();
		if (m_associatedWin)
		{
			m_associatedWin->redraw();
		}
	}
}

void ccPointListPickingDlg::markerSizeChanged(int size)
{
	if (size < 1 || !m_associatedWin)
		return;

	//display parameters
	ccGui::ParamStruct guiParams = m_associatedWin->getDisplayParameters();

	if (guiParams.labelMarkerSize != static_cast<unsigned>(size))
	{
		guiParams.labelMarkerSize = static_cast<unsigned>(size);
		m_associatedWin->setDisplayParameters(guiParams,m_associatedWin->hasOverriddenDisplayParameters());
		m_associatedWin->redraw();
	}
}

void ccPointListPickingDlg::exportToASCII(ExportFormat format)
{
	if (!m_associatedEntity)
		return;

	//get all labels
	std::vector<cc2DLabel*> labels;
	unsigned count = getPickedPoints(labels);
	if (count == 0)
		return;

	QSettings settings;
	settings.beginGroup("PointListPickingDlg");
	QString filename = settings.value("filename", "picking_list.csv").toString();
	settings.endGroup();

	filename = QFileDialog::getSaveFileName(this,
	                                        "Export to ASCII",
	                                        filename,
	                                        AsciiFilter::GetFileFilter());

	if (filename.isEmpty())
		return;

	settings.beginGroup("PointListPickingDlg");
	settings.setValue("filename", filename);
	settings.endGroup();

	QFile fp(filename);
	if (!fp.open(QFile::WriteOnly))
	{
		ccLog::Error(QString("Failed to open file '%1' for saving!").arg(filename));
		return;
	}

	//if a global shift exists, ask the user if it should be applied
	CCVector3d shift;
	double scale = 1.0;
	ccGenericPointCloud* asCloud = ccHObjectCaster::ToGenericPointCloud(m_associatedEntity);
	if (asCloud)
	{
		shift = asCloud->getGlobalShift();
		scale = asCloud->getGlobalScale();
	}

	if (shift.norm2() != 0 || scale != 1.0)
	{
		if (QMessageBox::warning(	this,
		                            "Apply global shift",
		                            "Do you want to apply global shift/scale to exported points?",
		                            QMessageBox::Yes | QMessageBox::No,
		                            QMessageBox::Yes ) == QMessageBox::No)
		{
			//reset shift
			shift = CCVector3d(0,0,0);
			scale = 1.0;
		}
	}

	//starting index
	unsigned startIndex = static_cast<unsigned>(std::max(0, startIndexSpinBox->value()));

	QTextStream stream(&fp);
	stream.setRealNumberPrecision(12);
    

	//object的第一个点的index
	int object_start_index = 0;
	//当前index的前一个index的标签
	QString preclass = labels[0]->getClass();
	for (unsigned i = 0; i < count; ++i)
	{

		assert(labels[i]->size() == 1);
		const cc2DLabel::PickedPoint& PP = labels[i]->getPickedPoint(0);
		CCVector3 P = PP.getPointPosition();
        // 由于combobox获得的cell中信息为上一index的内容，因此需要对i=0进行额外处理
		if(i == 0)
		    {

			    //stream << labels[i]->getName()<<',';
				stream << i<<',';
		        stream << labels[i]->getClass()<<',';
			    preclass = labels[i]->getClass();

			    stream	<< static_cast<double>(P.x) / scale - shift.x << ','
				    << static_cast<double>(P.y) / scale - shift.y << ','
				    << static_cast<double>(P.z) / scale - shift.z << endl;
			    continue;
		    }

    //9.9 添加内容 读取combobox
            QWidget* widget = tableWidget->cellWidget(i-1, 5);
		    QComboBox *combox = (QComboBox*)widget;
		    QString string = combox->currentText();
		    QString str = "YES";

		switch (format)
		{
		/*
		case PLP_ASCII_EXPORT_IXYZ:
			stream << i + startIndex << ',';
			break;
		case PLP_ASCII_EXPORT_GXYZ:
			stream << PP.index << ',';
			break;
		case PLP_ASCII_EXPORT_LXYZ:
			stream << labels[i]->getName() << ',';
			break;
        */
		case PLP_ASCII_EXPORT_LCXYZ:

			//9.9 添加内容 如果为闭环object 则最后保存的数据需要为第一个点数据
			if((QString::compare(preclass, labels[i]->getClass()) )&&(!QString::compare(string, str)))
			{
				//9.9 添加内容 保存第一个点
		        const cc2DLabel::PickedPoint& PP1 = labels[object_start_index]->getPickedPoint(0);
		        CCVector3 P1 = PP1.getPointPosition();
				
				stream << labels[object_start_index]->getName()<<',';
				stream << labels[object_start_index]->getClass()<<',';

				stream	<< static_cast<double>(P1.x) / scale - shift.x << ','
				<< static_cast<double>(P1.y) / scale - shift.y << ','
				<< static_cast<double>(P1.z) / scale - shift.z << ','
				<<'0'<<','
				<<'1'<<','
				<<'2'<<endl;
			}

			//9.8 增加内容 当obj发生变化，保存之后需要隔一行保存 同时更新obj的第一个点
		    if(QString::compare(preclass, labels[i]->getClass()))
			{
				stream<<endl;
			    object_start_index = i;
				//ccLog::Print(QString(object_start_index));
			}



			if(i == count-1)
			{
				QWidget* widget = tableWidget->cellWidget(i, 5);
		        QComboBox *combox = (QComboBox*)widget;
		        QString string = combox->currentText();

				if(!QString::compare(string, str))
				{
					stream <<i<<',';
			        stream << labels[i]->getClass()<<',';
                    preclass = labels[i]->getClass();

         			stream	<< static_cast<double>(P.x) / scale - shift.x << ','
		    		<< static_cast<double>(P.y) / scale - shift.y << ','
			    	<< static_cast<double>(P.z) / scale - shift.z << endl;

                    //9.9 添加内容 保存第一个点
		            const cc2DLabel::PickedPoint& PP1 = labels[object_start_index]->getPickedPoint(0);
		            CCVector3 P1 = PP1.getPointPosition();
				
				    //stream << labels[object_start_index]->getName()<<',';
				    stream <<object_start_index<<',';
					stream << labels[object_start_index]->getClass()<<',';

				    stream	<< static_cast<double>(P1.x) / scale - shift.x << ','
				    << static_cast<double>(P1.y) / scale - shift.y << ','
				    << static_cast<double>(P1.z) / scale - shift.z;

					continue;
				}
				else
				{
                    stream <<i<<',';
			        stream << labels[i]->getClass()<<',';
                    preclass = labels[i]->getClass();

         			stream	<< static_cast<double>(P.x) / scale - shift.x << ','
		    		<< static_cast<double>(P.y) / scale - shift.y << ','
			    	<< static_cast<double>(P.z) / scale - shift.z;

					continue;
				}
			}


			//正常情况下需要保存的内容格式
			{
		    //stream << labels[i]->getName()<<',';
		    stream <<i<<',';
			stream << labels[i]->getClass()<<',';
			preclass = labels[i]->getClass();

			stream	<< static_cast<double>(P.x) / scale - shift.x << ','
				<< static_cast<double>(P.y) / scale - shift.y << ','
				<< static_cast<double>(P.z) / scale - shift.z << endl;
			}

			break;

		default:
			//nothing to do
			break;
		}
		//9.9 删除代码
        /*
		stream	<< static_cast<double>(P.x) / scale - shift.x << ';'
				<< static_cast<double>(P.y) / scale - shift.y << ';'
				<< static_cast<double>(P.z) / scale - shift.z << endl;
	    */  
	}

	ccLog::Print(QString("[I/O] File '%1' saved successfully").arg(filename));
}

void ccPointListPickingDlg::updateList()
{
	//get all labels
	std::vector<cc2DLabel*> labels;
	const int count = static_cast<int>( getPickedPoints(labels) );

	const int oldRowCount = tableWidget->rowCount();

	revertToolButton->setEnabled(count);
	validToolButton->setEnabled(count);
	exportToolButton->setEnabled(count);
	countLineEdit->setText(QString::number(count));
	tableWidget->setRowCount(count);

	if ( count == 0 )
	{
		return;
	}

	// If we have any new rows, create QTableWidgetItems for them
	if ( (count - oldRowCount) > 0 )
	{
		for ( int i = oldRowCount; i < count; ++i )
		{
			tableWidget->setVerticalHeaderItem( i, new QTableWidgetItem );
/*
			for ( int j = 0; j < 4; ++j )
			{
				tableWidget->setItem( i, j, new QTableWidgetItem );
			}
*/
            for ( int j = 0; j < 6; ++j )
			{
				tableWidget->setItem( i, j, new QTableWidgetItem );
			}
		}
	}

	//starting index
	const int startIndex = startIndexSpinBox->value();
	const int precision = m_associatedWin ? static_cast<int>(m_associatedWin->getDisplayParameters().displayedNumPrecision) : 6;

	const bool showAbsolute = showGlobalCoordsCheckBox->isEnabled() && showGlobalCoordsCheckBox->isChecked();

	for ( int i = 0; i < count; ++i )
	{
		cc2DLabel* label = labels[static_cast<unsigned int>( i )];

		const cc2DLabel::PickedPoint& PP = label->getPickedPoint(0);
		CCVector3 P = PP.getPointPosition();
		CCVector3d Pd = (showAbsolute ? PP.cloudOrVertices()->toGlobal3d(P) : P);


        //add checkbox into the tablewidget
		QTableWidgetItem *checkbox = new QTableWidgetItem();
		checkbox->setCheckState(Qt::Unchecked);
		tableWidget->setItem(count-1, 0, checkbox);

        //9.9 添加内容

		//9.8 增加内容
		//添加下拉框
		
		if(i == count-1)
		{
			QStringList qstr; 
		    qstr<< "YES"<<"NO";
	        QComboBox *closedloop_comBox = new QComboBox();
		    closedloop_comBox->addItems(qstr);
		    tableWidget->setCellWidget(i, 5, closedloop_comBox);
		}
		
        
		//connect(tableWidget, SIGNAL(cellChanged(int, int)), this, SLOT(changeCheck(int row, int col)));
		//end
   
		//point index in list
		tableWidget->verticalHeaderItem( i )->setText( QStringLiteral( "%1" ).arg( i + startIndex ) );

		//update name as well
		if (	label->getUniqueID() > m_lastPreviousID
		    ||	label->getName().startsWith(s_defaultLabelBaseName) ) //DGM: we don't change the name of old labels that have a non-default name
		{
			label->setName(s_defaultLabelBaseName + QString::number(i+startIndex));
		}

		//point absolute index (in cloud)
		tableWidget->item( i, 0 )->setText( QStringLiteral( "%1" ).arg( PP.index ) );

		for ( int j = 0; j < 3; ++j )
		{
			tableWidget->item( i, j + 1 )->setText( QStringLiteral( "%1" ).arg( Pd.u[j], 0, 'f', precision ) );
		}
	}

	tableWidget->scrollToBottom();
}

void ccPointListPickingDlg::processPickedPoint(const PickedItem& picked)
{
	if (!picked.entity || picked.entity != m_associatedEntity || !MainWindow::TheInstance())
		return;

	cc2DLabel* newLabel = new cc2DLabel();
	if (picked.entity->isKindOf(CC_TYPES::POINT_CLOUD))
	{
		newLabel->addPickedPoint(static_cast<ccGenericPointCloud*>(picked.entity), picked.itemIndex, picked.entityCenter);
	}
	else if (picked.entity->isKindOf(CC_TYPES::MESH))
	{
		newLabel->addPickedPoint(static_cast<ccGenericMesh*>(picked.entity), picked.itemIndex, CCVector2d(picked.uvw.x, picked.uvw.y), picked.entityCenter);
	}
	else
	{
		delete newLabel;
		assert(false);
		return;
	}
	newLabel->setVisible(true);
	newLabel->setDisplayedIn2D(false);
	newLabel->displayPointLegend(true);
	newLabel->setCollapsed(true);
	ccGenericGLDisplay* display = m_associatedEntity->getDisplay();
	if (display)
	{
		newLabel->setDisplay(display);
		QSize size = display->getScreenSize();
		newLabel->setPosition(	static_cast<float>(picked.clickPoint.x() + 20) / size.width(),
		                        static_cast<float>(picked.clickPoint.y() + 20) / size.height() );
	}

	//add default container if necessary
	if (!m_orderedLabelsContainer)
	{
		m_orderedLabelsContainer = new ccHObject(s_pickedPointContainerName);
		m_associatedEntity->addChild(m_orderedLabelsContainer);
		m_orderedLabelsContainer->setDisplay(display);
		MainWindow::TheInstance()->addToDB(m_orderedLabelsContainer, false, true, false, false);
	}
	assert(m_orderedLabelsContainer);
	m_orderedLabelsContainer->addChild(newLabel);
	MainWindow::TheInstance()->addToDB(newLabel, false, true, false, false);
	m_toBeAdded.push_back(newLabel);

	//automatically send the new point coordinates to the clipboard
	QClipboard* clipboard = QApplication::clipboard();
	if (clipboard)
	{
		CCVector3 P = newLabel->getPickedPoint(0).getPointPosition();
		int precision = m_associatedWin ? m_associatedWin->getDisplayParameters().displayedNumPrecision : 6;
		int indexInList = startIndexSpinBox->value() + static_cast<int>(m_orderedLabelsContainer->getChildrenNumber()) - 1;
		clipboard->setText(QString("CC_POINT_#%0(%1;%2;%3)").arg(indexInList).arg(P.x, 0, 'f', precision).arg(P.y, 0, 'f', precision).arg(P.z, 0, 'f', precision));
	}

	updateList();

	if (m_associatedWin)
		m_associatedWin->redraw();
}

void ccPointListPickingDlg::setlabel()
{
	std::vector<cc2DLabel*> labels;

	
	QString dlgTitle="设置标签对话框";
        QString txtLabel="请输入标签：";
        QString defaultInput="";
        QLineEdit::EchoMode echoMode=QLineEdit::Normal;
        bool ok=false;
        QString text = QInputDialog::getText(this, dlgTitle, txtLabel, echoMode, defaultInput, &ok);



		const int count = static_cast<int>( getPickedPoints(labels) );
		for ( int i = 0; i < count; ++i )
		{
			if(tableWidget->item(i, 0)->checkState() == Qt::Checked)
			{
                tableWidget->item( i, 4 )->setText( text );
				labels[i]->setClass(text);
			}
			
		}

}

void ccPointListPickingDlg::exportToReferencePolyline()
{
	if (!m_associatedEntity)
		return;

	std::vector<cc2DLabel*> labels;
	unsigned count = getPickedPoints(labels);
	if (count == 0)
		return;

	QSettings settings;
	settings.beginGroup("PointListPickingDlg");
	QString filename = settings.value("filename", "picking_list.csv").toString();
	settings.endGroup();

	filename = QFileDialog::getSaveFileName(this,
	                                                                                        "Export to ASCII",
																							filename,
																							AsciiFilter::GetFileFilter());

	if(filename.isEmpty())
	            return;

	settings.beginGroup("PointListPickingDlg");
	settings.setValue("filename", filename);
	settings.endGroup();

	QFile fp(filename);
	if (!fp.open(QFile::WriteOnly))
	{
		ccLog::Error(QString("Failed to open file '%1' for saving!").arg(filename));
		return;		
	}

	//if a global shift exists, ask the user if it should be applied 
    CCVector3d shift;
	double scale = 1.0;
    ccGenericPointCloud* asCloud = ccHObjectCaster::ToGenericPointCloud(m_associatedEntity);
	if (asCloud)
	{
		shift = asCloud->getGlobalShift();
		scale = asCloud->getGlobalScale();
	}

	if (shift.norm2() != 0 || scale != 1.0)
	{
		if (QMessageBox::warning(	this,
		                            "Apply global shift",
		                            "Do you want to apply global shift/scale to exported points?",
		                            QMessageBox::Yes | QMessageBox::No,
		                            QMessageBox::Yes ) == QMessageBox::No)
		{
			//reset shift
			shift = CCVector3d(0,0,0);
			scale = 1.0;
		}
	}	

	QTextStream stream(&fp);
	stream.setRealNumberPrecision(12);

    std::vector<CPosition> Points_vec;

	for(int i = 0; i < labels.size(); i ++)
	{
		cc2DLabel::PickedPoint PP = labels[i]->getPickedPoint(0);
		CCVector3 P = PP.getPointPosition();
		CPosition Pyaw = CPosition(P);
		Points_vec.push_back(Pyaw);
	}
	
    //B三次样条
	int num = labels.size() ;
    std::vector<int> insertnum(num-1);

	for(int i = 0; i <num - 1; i ++)
	{
		cc2DLabel::PickedPoint PP1 = labels[i]->getPickedPoint(0);
		CCVector3 P1 = PP1.getPointPosition();
        
		cc2DLabel::PickedPoint PP2 = labels[i+1]->getPickedPoint(0);
		CCVector3 P2 = PP2.getPointPosition();

		int distance = sqrt((P1.x-P2.x)*(P1.x-P2.x)+(P1.y-P2.y)*(P1.y-P2.y));
		insertnum[i] = distance/4;//每两个点间添加的点的数量
	}

	insertnum.push_back(sqrt( (labels[0]->getPickedPoint(0).getPointPosition().x-labels[num-1]->getPickedPoint(0).getPointPosition().x)*(labels[0]->getPickedPoint(0).getPointPosition().x-labels[num-1]->getPickedPoint(0).getPointPosition().x)+(labels[0]->getPickedPoint(0).getPointPosition().y-labels[num-1]->getPickedPoint(0).getPointPosition().y)*(labels[0]->getPickedPoint(0).getPointPosition().y-labels[num-1]->getPickedPoint(0).getPointPosition().y) )/4);

	QWidget* widget = tableWidget->cellWidget(num-1, 5);
	QComboBox *combox = (QComboBox*)widget;
	QString string = combox->currentText();
    QString str = "YES";
	bool isClosed = !QString::compare(string, str);
    
	ThreeOrderBSplineInterpolatePt(Points_vec, num, insertnum, isClosed);

    ComputeYaw(Points_vec);

	for(int i = 0; i < Points_vec.size(); i ++)
	{
		CPosition PPyaw = Points_vec[i];

		stream << i <<',';
		stream << i << ',';//poi_type
		stream << static_cast<double>(PPyaw.p.x)/scale - shift.x << ','
		               << static_cast<double>(PPyaw.p.y)/scale - shift.y << ','
					   << static_cast<double>(PPyaw.p.z)/scale - shift.z << ','
					   <<PPyaw.yaw<<','//point_yaw
					   <<i<<','//s
					   <<i<<','//lng
					   <<i <<','//lat
					   <<i <<','//alt
					   <<i << endl;//gear
	}
}

void ccPointListPickingDlg::ThreeOrderBSplineInterpolatePt(std::vector<CPosition>& Points_vec, int Num, std::vector<int> InsertNum, bool isClosed)
{
	if(Points_vec.empty()||InsertNum.size() == 0) return;

	int InsertNumSum = 0;
	for(int i = 0; i < Num - 1; i ++) InsertNumSum += InsertNum[i];

	std::vector<CPosition> Points_vec_new(Num + 2);

	for(int i = 0; i < Num; i ++)
	{
		Points_vec_new[i+1] = Points_vec[i]; 
	}
    
   if(isClosed)
   {
        Points_vec_new[0].p.x = Points_vec_new[Num].p.x;
	    Points_vec_new[0].p.y = Points_vec_new[Num].p.y;
        Points_vec_new[0].p.z = Points_vec_new[1].p.z;

    	Points_vec_new[Num + 1].p.x = Points_vec_new[1].p.x;
        Points_vec_new[Num + 1].p.y = Points_vec_new[1].p.y;
        Points_vec_new[Num + 1].p.z = Points_vec_new[1].p.z;
   }else
   {
        Points_vec_new[0].p.x = 2*Points_vec_new[1].p.x - Points_vec_new[2].p.x;
    	Points_vec_new[0].p.y = 2*Points_vec_new[1].p.y - Points_vec_new[2].p.y;
        Points_vec_new[0].p.z = Points_vec_new[1].p.z;

    	Points_vec_new[Num + 1].p.x = 2*Points_vec_new[Num].p.x - Points_vec_new[Num-1].p.x;
        Points_vec_new[Num + 1].p.y = 2*Points_vec_new[Num].p.y - Points_vec_new[Num-1].p.y;
        Points_vec_new[Num + 1].p.z = Points_vec_new[Num].p.z;
   }

    CCVector3 NodePt1, NodePt2, NodePt3, NodePt4;
	double t;

	Points_vec.resize(Num + InsertNumSum);
	
	int totalnum = 0;

	for(int i = 0; i < Num - 1; i ++)
	{
		NodePt1 = Points_vec_new[i].p;
		NodePt2 = Points_vec_new[i+1].p;
		NodePt3 = Points_vec_new[i+2].p;
		NodePt4 = Points_vec_new[i+3].p;
		double dt = 1.0/(InsertNum[i] + 1);
        
		for(int j = 0; j < InsertNum[i] + 1; j ++)
		{
			t = dt*j;
			Points_vec[totalnum].p.x = F03(t)*NodePt1.x + F13(t)*NodePt2.x + F23(t)*NodePt3.x + F33(t)*NodePt4.x;
			Points_vec[totalnum].p.y = F03(t)*NodePt1.y + F13(t)*NodePt2.y + F23(t)*NodePt3.y + F33(t)*NodePt4.y;
			Points_vec[totalnum].p.z = Points_vec_new[0].p.z;
			totalnum ++;
		}
    
		if(i == Num - 2)
		{
			t = 1;
			Points_vec[totalnum].p.x = F03(t)*NodePt1.x + F13(t)*NodePt2.x + F23(t)*NodePt3.x + F33(t)*NodePt4.x;
			Points_vec[totalnum].p.y = F03(t)*NodePt1.y + F13(t)*NodePt2.y + F23(t)*NodePt3.y + F33(t)*NodePt4.y;
			Points_vec[totalnum].p.z = Points_vec_new[0].p.z;
			totalnum ++;
		}
		
	}
    
	if(isClosed)
	{
		Points_vec.resize(Num + InsertNumSum + InsertNum[Num-1]-1);
        NodePt1 = Points_vec_new[Num-1].p;
	    NodePt2 = Points_vec_new[Num].p;
	    NodePt3 = Points_vec_new[1].p;
	    NodePt4 = Points_vec_new[2].p;
	    double dt = 1.0/(InsertNum[Num-1] + 1);

        for(int j = 1; j < InsertNum[Num-1] + 1; j ++)
    	{
    		t = dt*j;
    		Points_vec[totalnum].p.x = F03(t)*NodePt1.x + F13(t)*NodePt2.x + F23(t)*NodePt3.x + F33(t)*NodePt4.x;
    		Points_vec[totalnum].p.y = F03(t)*NodePt1.y + F13(t)*NodePt2.y + F23(t)*NodePt3.y + F33(t)*NodePt4.y;
    		Points_vec[totalnum].p.z = Points_vec_new[0].p.z;
    		totalnum ++;
    	}
	}
}
double ccPointListPickingDlg::F03(double t)
{
	return 1.0/6*(-t*t*t+3*t*t-3*t+1);
}
double ccPointListPickingDlg::F13(double t)
{
	return 1.0/6*(3*t*t*t-6*t*t+4);
}
double ccPointListPickingDlg::F23(double t)
{
	return 1.0/6*(-3*t*t*t+3*t*t+3*t+1);
}
double ccPointListPickingDlg::F33(double t)
{
	return 1.0/6*t*t*t;
}

void ccPointListPickingDlg::ComputeYaw(std::vector<CPosition>& Points_vec)
{
	for(int i = 0; i < Points_vec.size()-1; i ++)
	{
		CPosition P1 = Points_vec[i];

		
        CPosition P2 = Points_vec[i+1];

		CCVector3 p1 = P1.p;
		CCVector3 p2 = P2.p;
        double x = (p1.y-p2.y)/(p1.x-p2.x);
		double yyaw = atan(x);

		Points_vec[i].yaw = yyaw;
	}

	CPosition P1 = Points_vec[Points_vec.size()-1];
    CPosition P2 = Points_vec[0];
    
	CCVector3 p1 = P1.p;
	CCVector3 p2 = P2.p;
    double x = (p1.y-p2.y)/(p1.x-p2.x);
	double yyaw = atan(x);

	Points_vec[Points_vec.size()-1].yaw = yyaw;

}