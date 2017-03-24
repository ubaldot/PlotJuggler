#include "filterablelistwidget.h"
#include "ui_filterablelistwidget.h"
#include <QDebug>
#include <QLayoutItem>
#include <QMenu>
#include <QSettings>
#include <QDrag>
#include <QMimeData>
#include <QHeaderView>
#include <QFontDatabase>
#include <QMessageBox>
#include <QApplication>
#include <QPainter>
#include <QCompleter>

class TreeModelCompleter : public QCompleter
{

public:
    TreeModelCompleter(QAbstractItemModel *model, QObject *parent = 0): QCompleter(model, parent)
    {  }


    QStringList splitPath(const QString &path) const override
    {
        return path.split('.');
    }

    QString pathFromIndex(const QModelIndex &index) const override
    {
        QStringList dataList;
        for (QModelIndex i = index; i.isValid(); i = i.parent())
        {
            QString name = model()->data(i, completionRole()).toString();
            dataList.prepend(name);
        }
        return dataList.join('.');
    }
};

FilterableListWidget::FilterableListWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FilterableListWidget),
    _string_model( new QStandardItemModel(this)),
    _completer( new TreeModelCompleter(_string_model, this) )
{
    ui->setupUi(this);
    ui->tableWidget->viewport()->installEventFilter( this );
    ui->lineEdit->installEventFilter( this );

    table()->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table()->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table()->horizontalHeader()->resizeSection(1, 120);

    ui->widgetOptions->setVisible(false);

    ui->radioRegExp->setAutoExclusive(true);
    ui->radioContains->setAutoExclusive(true);
    ui->radioPrefix->setAutoExclusive(true);

    _completer->setCompletionMode( QCompleter::PopupCompletion );

}

FilterableListWidget::~FilterableListWidget()
{
    delete ui;
}

int FilterableListWidget::rowCount() const
{
    return getTtable()->rowCount();
}

void FilterableListWidget::clear()
{
    table()->setRowCount(0);
    ui->labelNumberDisplayed->setText( "0 of 0");
}

void FilterableListWidget::addItem(QTableWidgetItem *item)
{
    const int row = rowCount();
    table()->setRowCount(row+1);
    table()->setItem(row, 0, item);

    auto val_cell = new QTableWidgetItem("-");
    val_cell->setTextAlignment(Qt::AlignRight);
    val_cell->setFlags( Qt::NoItemFlags | Qt::ItemIsEnabled );
    val_cell->setFont(  QFontDatabase::systemFont(QFontDatabase::FixedFont) );

    table()->setItem(row, 1, val_cell );

    QString name = item->data(Qt::DisplayRole).toString();
    QStringList parts = name.split('.');

    QStandardItem *parent_item = _string_model->invisibleRootItem();

    for (int col=0; col < parts.count(); col++)
    {
        bool present = false;
        for (int row = 0; row < parent_item->rowCount(); row++)
        {
            if( parent_item->child(row)->text() == parts[col])
            {
                present = true;
                parent_item = parent_item->child(row);
                break;
            }
        }
        if( !present )
        {
            QStandardItem *item = new QStandardItem(parts[col]);
            parent_item->appendRow(item);
            parent_item = item;
        }
    }
}


QList<int>
FilterableListWidget::findRowsByName(const QString &text) const
{
    QList<int> output;
    QList<QTableWidgetItem*> item_list = getTtable()->findItems( text, Qt::MatchExactly);
    for(QTableWidgetItem* item : item_list)
    {
        if(item->column() == 0) {
            output.push_back( item->row() );
        }
    }
    return output;
}

const QTableWidget *FilterableListWidget::getTtable() const
{
    return ui->tableWidget;
}

QTableWidget *FilterableListWidget::table()
{
    return ui->tableWidget;
}

void FilterableListWidget::updateFilter()
{
    on_lineEdit_textChanged( ui->lineEdit->text() );
}

void FilterableListWidget::keyPressEvent(QKeyEvent *event)
{
    if( event->key() == Qt::Key_Delete){
        removeSelectedCurves();
    }
}

bool FilterableListWidget::eventFilter(QObject *object, QEvent *event)
{
    QObject *obj = object;
    while ( obj != NULL )
    {
        if( obj == table() || obj == ui->lineEdit ) break;
        obj = obj->parent();
    }

    if(obj == table())
    {
        if(event->type() == QEvent::MouseButtonPress)
        {
            QMouseEvent *mouse_event = static_cast<QMouseEvent*>(event);
            if(mouse_event->button() == Qt::LeftButton )
            {
                _newX_modifier = false;
                _drag_start_pos = mouse_event->pos();
            }
            else if(mouse_event->button() == Qt::RightButton )
            {
                _newX_modifier = true;
                _drag_start_pos = mouse_event->pos();
            }
        }
        else if(event->type() == QEvent::MouseMove)
        {
            QMouseEvent *mouse_event = static_cast<QMouseEvent*>(event);
            double distance_from_click = (mouse_event->pos() - _drag_start_pos).manhattanLength();

            if ((mouse_event->buttons() == Qt::LeftButton || mouse_event->buttons() == Qt::RightButton) &&
                    distance_from_click >= QApplication::startDragDistance())
            {
                QDrag *drag = new QDrag(this);
                QMimeData *mimeData = new QMimeData;

                QByteArray mdata;
                QDataStream stream(&mdata, QIODevice::WriteOnly);

                for(QTableWidgetItem* item: table()->selectedItems()) {
                    stream << item->text();
                }
                if( _newX_modifier )
                {
                    if( table()->selectedItems().size() == 1)
                    {
                        mimeData->setData("curveslist/new_X_axis", mdata);

                        QPixmap cursor( QSize(160,30) );
                        cursor.fill();

                        QPainter painter;
                        painter.begin( &cursor);
                        painter.setPen(QColor(22, 22, 22));

                        QString text("set as new X axis");
                        painter.setFont( QFont("Arial", 14 ) );

                        painter.drawText( QRect(0, 0, 160, 30), Qt::AlignHCenter | Qt::AlignVCenter, text );
                        painter.end();

                        drag->setDragCursor(cursor, Qt::MoveAction);
                    }
                    else{
                        //abort
                        QWidget::eventFilter(object,event);
                    }
                }
                else{
                    mimeData->setData("curveslist/add_curve", mdata);
                }

                drag->setMimeData(mimeData);
                drag->exec(Qt::CopyAction | Qt::MoveAction);
            }
        }
    }

//    if( obj == ui->lineEdit )
//    {
//        if(event->type() == QEvent::KeyPress)
//        {
//            QKeyEvent *key_event = static_cast<QKeyEvent*>(event);
//            if( key_event->key() | Qt::Key_Tab || key_event->key() | Qt::Key_Enter)
//            {
//                qDebug() << "key event" ;
//                if( _completer->completionCount() == 1)
//                {
//                    qDebug() << "pathFromIndex "  <<
//                    _completer->pathFromIndex( _completer->currentIndex() );
//                }
//            }
//        }
//    }
    return QWidget::eventFilter(object,event);
}


void FilterableListWidget::on_radioContains_toggled(bool checked)
{
    if(checked) {
        updateFilter();
        ui->lineEdit->setCompleter( nullptr );
    }
}

void FilterableListWidget::on_radioRegExp_toggled(bool checked)
{
    if(checked) {
        updateFilter();
        ui->lineEdit->setCompleter( nullptr );
    }
}

void FilterableListWidget::on_radioPrefix_toggled(bool checked)
{
    if(checked) {
        updateFilter();
        ui->lineEdit->setCompleter( _completer );
    }
}

void FilterableListWidget::on_checkBoxCaseSensitive_toggled(bool checked)
{
    updateFilter();
}


void FilterableListWidget::on_lineEdit_textChanged(const QString &search_string)
{
    int item_count = rowCount();
    int visible_count = 0;
    bool updated = false;

    Qt::CaseSensitivity cs = Qt::CaseInsensitive;
    if( ui->checkBoxCaseSensitive->isChecked())
    {
        cs = Qt::CaseSensitive;
    }
    QRegExp regexp( search_string,  cs, QRegExp::Wildcard );
    QRegExpValidator v(regexp, 0);

    for (int row=0; row< rowCount(); row++)
    {
        QTableWidgetItem* item = table()->item(row,0);
        QString name = item->text();
        int pos = 0;
        bool toHide = false;

        if( ui->radioRegExp->isChecked())
        {
            toHide = v.validate( name, pos ) != QValidator::Acceptable;
        }
        else if( ui->radioPrefix->isChecked())
        {
            toHide = !name.startsWith( search_string, cs ) ;
        }
        else if( ui->radioContains->isChecked())
        {
            QStringList items = search_string.split(' ');
            for (int i=0; i< items.size(); i++)
            {
                if( name.contains(items[i], cs) == false )
                {
                    toHide = true;
                }
            }
        }
        if( !toHide ) visible_count++;

        if( toHide != table()->isRowHidden(row) ) updated = true;

        table()->setRowHidden(row, toHide );
    }
    ui->labelNumberDisplayed->setText( QString::number( visible_count ) + QString(" of ") + QString::number( item_count ) );

    if(updated){
        emit hiddenItemsChanged();
    }
}

void FilterableListWidget::on_pushButtonSettings_toggled(bool checked)
{
    ui->widgetOptions->setVisible(checked);
}

void FilterableListWidget::on_checkBoxHideSecondColumn_toggled(bool checked)
{
    if(checked){
        table()->hideColumn(1);
        emit hiddenItemsChanged();
    }
    else{
        table()->showColumn(1);
        emit hiddenItemsChanged();
    }
}

void FilterableListWidget::removeSelectedCurves()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(0, tr("Warning"),
                                  tr("Do you really want to remove these data?\n"),
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No );

    if( reply == QMessageBox::Yes ) {

        while( table()->selectedItems().size() > 0 )
        {
            QTableWidgetItem* item = table()->selectedItems().first();
            emit deleteCurve( item->text() );
        }
    }
    updateFilter();
}

void FilterableListWidget::removeRow(int row)
{
    table()->removeRow(row);
}


