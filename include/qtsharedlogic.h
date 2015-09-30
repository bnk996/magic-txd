#pragma once

#include <QPushButton>
#include <QLineEdit>
#include <QFileDialog>
#include <QHBoxLayout>

namespace qtshared
{
    struct PathBrowseButton : public QPushButton
    {
        inline PathBrowseButton( QString text, QLineEdit *lineEdit ) : QPushButton( text )
        {
            this->theEdit = lineEdit;

            connect( this, &QPushButton::clicked, this, &PathBrowseButton::OnBrowsePath );
        }

    public slots:
        void OnBrowsePath( bool checked )
        {
            filePath curPathAbs;

            bool hasPath = fileRoot->GetFullPath( "/", false, curPathAbs );

            QString selPath =
                QFileDialog::getExistingDirectory(
                    this, "Browse Directory...",
                    hasPath ? QString::fromStdWString( curPathAbs.convert_unicode() ) : QString()
                );

            if ( selPath.isEmpty() == false )
            {
                this->theEdit->setText( selPath );
            }
        }

    private:
        QLineEdit *theEdit;
    };

    inline QLayout* createPathSelectGroup( QString begStr, QLineEdit*& editOut )
    {
        QHBoxLayout *pathRow = new QHBoxLayout();

        QLineEdit *pathEdit = new QLineEdit( begStr );

        pathRow->addWidget( pathEdit );

        QPushButton *pathBrowseButton = new PathBrowseButton( "Browse", pathEdit );

        pathRow->addWidget( pathBrowseButton );

        editOut = pathEdit;

        return pathRow;
    }
};