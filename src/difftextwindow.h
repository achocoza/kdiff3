/***************************************************************************
 *   Copyright (C) 2003-2007 by Joachim Eibl                               *
 *   joachim.eibl at gmx.de                                                *
 *   Copyright (C) 2018 Michael Reeves reeves.87@gmail.com                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef DIFFTEXTWINDOW_H
#define DIFFTEXTWINDOW_H

#include "diff.h"

#include <QLabel>

class QMenu;
class QStatusBar;
class Options;
class DiffTextWindowData;
class DiffTextWindowFrame;
class EncodingLabel;
class RLPainter;

class DiffTextWindow : public QWidget
{
   Q_OBJECT
public:
   DiffTextWindow(DiffTextWindowFrame* pParent, QStatusBar* pStatusBar, Options* pOptions, e_SrcSelector winIdx);
   ~DiffTextWindow() override;
   void init(
      const QString& fileName,
      QTextCodec* pTextCodec,
      e_LineEndStyle eLineEndStyle,
      const QVector<LineData>* pLineData,
      int size,
      const Diff3LineVector* pDiff3LineVector,
      const ManualDiffHelpList* pManualDiffHelpList,
      bool bTriple
      );
   void reset();
   void convertToLinePos( int x, int y, LineRef& line, int& pos );

   QString getSelection();
   int getFirstLine();
   int calcTopLineInFile( int firstLine );

   int getMaxTextWidth();
   LineCount getNofLines();
   int getNofVisibleLines();
   int getVisibleTextAreaWidth();

   int convertLineToDiff3LineIdx( LineRef line );
   LineRef convertDiff3LineIdxToLine( int d3lIdx );

   void convertD3LCoordsToLineCoords( int d3LIdx, int d3LPos, int& line, int& pos );
   void convertLineCoordsToD3LCoords( int line, int pos, int& d3LIdx, int& d3LPos );

   void convertSelectionToD3LCoords();

   bool findString( const QString& s, LineRef& d3vLine, int& posInLine, bool bDirDown, bool bCaseSensitive );
   void setSelection( LineRef firstLine, int startPos, LineRef lastLine, int endPos, LineRef& l, int& p );
   void getSelectionRange( LineRef* firstLine, LineRef* lastLine, e_CoordType coordType );

   void setPaintingAllowed( bool bAllowPainting );
   void recalcWordWrap( bool bWordWrap, int wrapLineVectorSize, int visibleTextWidth);
   void recalcWordWrapHelper( int wrapLineVectorSize, int visibleTextWidth, int cacheListIdx);
   void print( RLPainter& painter, const QRect& r, int firstLine, int nofLinesPerPage );
Q_SIGNALS:
   void resizeHeightChangedSignal(int nofVisibleLines);
   void resizeWidthChangedSignal(int nofVisibleColumns);
   void scrollDiffTextWindow( int deltaX, int deltaY );
   void newSelection();
   void selectionEnd();
   void setFastSelectorLine( LineIndex line );
   void gotFocus();
   void lineClicked( e_SrcSelector winIdx, int line );

public Q_SLOTS:
   void setFirstLine( int line );
   void setHorizScrollOffset( int horizScrollOffset );
   void resetSelection();
   void setFastSelectorRange( int line1, int nofLines );

protected:
   void mousePressEvent ( QMouseEvent * ) override;
   void mouseReleaseEvent ( QMouseEvent * ) override;
   void mouseMoveEvent ( QMouseEvent * ) override;
   void mouseDoubleClickEvent ( QMouseEvent * e ) override;

   void paintEvent( QPaintEvent*  ) override;
   void dragEnterEvent( QDragEnterEvent* e ) override;
   void focusInEvent( QFocusEvent* e ) override;

   void resizeEvent( QResizeEvent* ) override;
   void timerEvent(QTimerEvent*) override;

private:
   DiffTextWindowData* d;
   void showStatusLine( int line );
   friend class DiffTextWindowFrame;
};


class DiffTextWindowFrameData;

class DiffTextWindowFrame : public QWidget
{
   Q_OBJECT
public:
   DiffTextWindowFrame( QWidget* pParent, QStatusBar* pStatusBar, Options* pOptions, e_SrcSelector winIdx, SourceData* psd);
   ~DiffTextWindowFrame() override;
   DiffTextWindow* getDiffTextWindow();
   void init();
   void setFirstLine(int firstLine);
   void sendEncodingChangedSignal(QTextCodec* c);
Q_SIGNALS:
   void fileNameChanged(const QString&, e_SrcSelector);
   void encodingChanged(QTextCodec*);
protected:
   bool eventFilter( QObject*, QEvent* ) override;
   //void paintEvent(QPaintEvent*);
private Q_SLOTS:
   void slotReturnPressed();
   void slotBrowseButtonClicked();

 private:
   DiffTextWindowFrameData* d;
};

class EncodingLabel : public QLabel
{
   Q_OBJECT
public:
   EncodingLabel( const QString & text, DiffTextWindowFrame* pDiffTextWindowFrame, SourceData* psd, Options* pOptions);
protected:
   void mouseMoveEvent(QMouseEvent *ev) override;
   void mousePressEvent(QMouseEvent *ev) override;
private Q_SLOTS:
   void slotEncodingChanged();
private:
   DiffTextWindowFrame* m_pDiffTextWindowFrame; //To send "EncodingChanged" signal
   QMenu* m_pContextEncodingMenu;
   SourceData* m_pSourceData; //SourceData to get access to "isEmpty()" and "isFromBuffer()" functions
   static const int m_maxRecentEncodings  = 5;
   Options* m_pOptions;

   void insertCodec( const QString& visibleCodecName, QTextCodec* pCodec, QList<int> &CodecEnumList, QMenu* pMenu, int currentTextCodecEnum);
};

bool startRunnables();

#endif

