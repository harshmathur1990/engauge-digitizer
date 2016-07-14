/******************************************************************************************************
 * (C) 2014 markummitchell@github.com. This file is part of Engauge Digitizer, which is released      *
 * under GNU General Public License version 2 (GPLv2) or (at your option) any later version. See file *
 * LICENSE or go to gnu.org/licenses for details. Distribution requires prior written permission.     *
 ******************************************************************************************************/

#include "DlgEditPointCurve.h"
#include "DlgValidatorAbstract.h"
#include "DlgValidatorFactory.h"
#include "DocumentModelCoords.h"
#include "FormatCoordsUnits.h"
#include "Logger.h"
#include "MainWindow.h"
#include "MainWindowModel.h"
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include "QtToString.h"
#include <QVBoxLayout>
#include "Transformation.h"

const Qt::Alignment ALIGNMENT = Qt::AlignCenter;

const int MIN_WIDTH_TO_FIT_STRANGE_UNITS = 200;

DlgEditPointCurve::DlgEditPointCurve (MainWindow &mainWindow,
                                      const DocumentModelCoords &modelCoords,
                                      const MainWindowModel &modelMainWindow,
                                      const Transformation &transformation,
                                      const double *xInitialValue,
                                      const double *yInitialValue) :
  QDialog (&mainWindow),
  m_changed (false),
  m_modelCoords (modelCoords),
  m_modelMainWindow (modelMainWindow)
{
  LOG4CPP_INFO_S ((*mainCat)) << "DlgEditPointCurve::DlgEditPointCurve";

  QVBoxLayout *layout = new QVBoxLayout;
  setLayout (layout);

  setCursor  (QCursor (Qt::ArrowCursor));
  setModal(true);
  setWindowTitle (tr ("Edit Curve Point(s)"));

  createCoords (layout);
  createHint (layout);
  createOkCancel (layout);

  initializeGraphCoordinates (xInitialValue,
                              yInitialValue,
                              transformation);

  m_changed = false; // Initialization of coordinate vaues changed this flag so we reset it and update the controls
  updateControls ();
}

DlgEditPointCurve::~DlgEditPointCurve()
{
  LOG4CPP_INFO_S ((*mainCat)) << "DlgEditPointCurve::~DlgEditPointCurve";
}

void DlgEditPointCurve::createCoords (QVBoxLayout *layoutOuter)
{
  // Constraints on x and y are needed for log scaling
  bool isConstraintX = (m_modelCoords.coordScaleXTheta() == COORD_SCALE_LOG);
  bool isConstraintY = (m_modelCoords.coordScaleYRadius() == COORD_SCALE_LOG);
  DlgValidatorFactory dlgValidatorFactory;
  m_validatorGraphX = dlgValidatorFactory.createCartesianOrPolarWithPolarPolar (m_modelCoords.coordScaleXTheta(),
                                                                                isCartesian (),
                                                                                m_modelCoords.coordUnitsX(),
                                                                                m_modelCoords.coordUnitsTheta(),
                                                                                m_modelCoords.coordUnitsDate(),
                                                                                m_modelCoords.coordUnitsTime(),
                                                                                m_modelMainWindow.locale());
  m_validatorGraphY = dlgValidatorFactory.createCartesianOrPolarWithNonPolarPolar (m_modelCoords.coordScaleYRadius(),
                                                                                   isCartesian (),
                                                                                   m_modelCoords.coordUnitsY(),
                                                                                   m_modelCoords.coordUnitsRadius(),
                                                                                   m_modelCoords.coordUnitsDate(),
                                                                                   m_modelCoords.coordUnitsTime(),
                                                                                   m_modelMainWindow.locale());

  // Label, with guidance in terms of legal ranges and units
  QString description = QString ("%1 (%2, %3)%4%5%6%7%8%9 %10:")
                        .arg (tr ("Graph Coordinates"))
                        .arg (nameXTheta ())
                        .arg (nameYRadius ())
                        .arg (isConstraintX || isConstraintY ? " with " : "")
                        .arg (isConstraintX                  ? QString (nameXTheta ()) : "")
                        .arg (isConstraintX                  ? " > 0" : "")
                        .arg (isConstraintX && isConstraintY ? " and " : "")
                        .arg (                 isConstraintY ? QString (nameYRadius ()) : "")
                        .arg (                 isConstraintY ? " > 0" : "")
                        .arg (tr ("as"));
  QGroupBox *panel = new QGroupBox (description, this);
  layoutOuter->addWidget (panel);

  QHBoxLayout *layout = new QHBoxLayout (panel);
  panel->setLayout (layout);

  // Row
  QLabel *labelGraphParLeft = new QLabel (tr ("("), this);
  layout->addWidget(labelGraphParLeft, 0);

  m_editGraphX = new QLineEdit;
  m_editGraphX->setMinimumWidth(MIN_WIDTH_TO_FIT_STRANGE_UNITS);
  m_editGraphX->setAlignment (ALIGNMENT);
  m_editGraphX->setValidator (m_validatorGraphX);
  // setStatusTip does not work for modal dialogs
  m_editGraphX->setWhatsThis (tr ("Enter the first graph coordinate of the axis point.\n\n"
                                  "For cartesian plots this is X. For polar plots this is the radius R.\n\n"
                                  "The expected format of the coordinate value is determined by the locale setting. If "
                                  "typed values are not recognized as expected, check the locale setting in Settings / Main Window..."));
  layout->addWidget(m_editGraphX, 0);
  connect (m_editGraphX, SIGNAL (textChanged (const QString &)), this, SLOT (slotTextChanged (const QString &)));

  QLabel *labelGraphComma = new QLabel (tr (", "), this);
  layout->addWidget(labelGraphComma, 0);

  m_editGraphY = new QLineEdit;
  m_editGraphY->setMinimumWidth(MIN_WIDTH_TO_FIT_STRANGE_UNITS);
  m_editGraphY->setAlignment (ALIGNMENT);
  m_editGraphY->setValidator (m_validatorGraphY);
  // setStatusTip does not work for modal dialogs
  m_editGraphY->setWhatsThis (tr ("Enter the second graph coordinate of the axis point.\n\n"
                                  "For cartesian plots this is Y. For plot plots this is the angle Theta.\n\n"
                                  "The expected format of the coordinate value is determined by the locale setting. If "
                                  "typed values are not recognized as expected, check the locale setting in Settings / Main Window..."));
  layout->addWidget(m_editGraphY, 0);
  connect (m_editGraphY, SIGNAL (textChanged (const QString &)), this, SLOT (slotTextChanged (const QString &)));

  QLabel *labelGraphParRight = new QLabel (tr (")"), this);
  layout->addWidget(labelGraphParRight, 0);
}

void DlgEditPointCurve::createHint (QVBoxLayout *layoutOuter)
{
  // Insert a hint explaining why decimal points may not be accepted. Very confusing for user to figure out the problem at first, and
  // then figure out which setting should change to fix it. The hint is centered so it is slightly less intrusive

  QWidget *widget = new QWidget;
  layoutOuter->addWidget (widget, 0, Qt::AlignCenter);

  QHBoxLayout *layout = new QHBoxLayout;
  widget->setLayout (layout);

  QString locale = QLocaleToString (m_modelMainWindow.locale ());
  QString hint = QString ("%1: %2")
                 .arg (tr ("Number format"))
                 .arg (locale);
  QLabel *label = new QLabel (hint);
  layout->addWidget (label);
}

void DlgEditPointCurve::createOkCancel (QVBoxLayout *layoutOuter)
{
  QWidget *panel = new QWidget (this);
  layoutOuter->addWidget (panel, 0, Qt::AlignCenter);

  QHBoxLayout *layout = new QHBoxLayout (panel);
  panel->setLayout (layout);

  m_btnOk = new QPushButton (tr ("Ok"), this);
  layout->addWidget(m_btnOk);
  connect (m_btnOk, SIGNAL (released ()), this, SLOT (accept ()));

  m_btnCancel = new QPushButton (tr ("Cancel"), this);
  layout->addWidget(m_btnCancel);
  connect (m_btnCancel, SIGNAL (released ()), this, SLOT (reject ()));
}

void DlgEditPointCurve::initializeGraphCoordinates (const double *xInitialValue,
                                                    const double *yInitialValue,
                                                    const Transformation &transformation)
{
  LOG4CPP_INFO_S ((*mainCat)) << "DlgEditPointCurve::initializeGraphCoordinates";

  QString xTheta, yRadius;
  if ((xInitialValue != 0) &&
      (yInitialValue != 0)) {

    FormatCoordsUnits format;
    format.unformattedToFormatted (*xInitialValue,
                                   *yInitialValue,
                                   m_modelCoords,
                                   m_modelMainWindow,
                                   xTheta,
                                   yRadius,
                                   transformation);
  }

  m_editGraphX->setText (xTheta);
  m_editGraphY->setText (yRadius);
}

bool DlgEditPointCurve::isCartesian () const
{
  return (m_modelCoords.coordsType() == COORDS_TYPE_CARTESIAN);
}

QChar DlgEditPointCurve::nameXTheta () const
{
  return (isCartesian () ? QChar ('X') : THETA);
}

QChar DlgEditPointCurve::nameYRadius () const
{
  return (isCartesian () ? QChar ('Y') : QChar ('R'));
}

QPointF DlgEditPointCurve::posGraph () const
{
  double xTheta, yRadius;

  FormatCoordsUnits format;

  format.formattedToUnformatted (m_editGraphX->text(),
                                 m_editGraphY->text(),
                                 m_modelCoords,
                                 m_modelMainWindow,
                                 xTheta,
                                 yRadius);

  return QPointF (xTheta,
                  yRadius);
}

void DlgEditPointCurve::slotTextChanged (const QString &)
{
  m_changed = true;
  updateControls ();
}

QString DlgEditPointCurve::unitsType (bool isXTheta) const
{
  if (isCartesian ()) {
    if (isXTheta) {
      return coordUnitsNonPolarThetaToBriefType (m_modelCoords.coordUnitsX());
    } else {
      return coordUnitsNonPolarThetaToBriefType (m_modelCoords.coordUnitsY());
    }
  } else {
    if (isXTheta) {
      return coordUnitsPolarThetaToBriefType (m_modelCoords.coordUnitsTheta());
    } else {
      return coordUnitsNonPolarThetaToBriefType (m_modelCoords.coordUnitsRadius());
    }
  }
}

void DlgEditPointCurve::updateControls ()
{
  QString textX = m_editGraphX->text();
  QString textY = m_editGraphY->text();

  // Feedback indicating that empty coordinate will be skipped rather than applied to the selected points
  QString colorX = (textX.isEmpty() ? QString ("lightGray") : QString ("white"));
  QString colorY = (textY.isEmpty() ? QString ("lightGray") : QString ("white"));
  QString styleX = QString ("QLineEdit { background-color: %1; }").arg (colorX);
  QString styleY = QString ("QLineEdit { background-color: %1; }").arg (colorY);
  m_editGraphX->setStyleSheet (styleX);
  m_editGraphY->setStyleSheet (styleY);

  // Tests that all have to be true
  // 1) At least one value has been changed
  // 2) At least one value is not empty
  // 3) The values that are not empty are properly formatted. This is done remembering that we need to
  //    check for not empty (which allows single minus sign) and for valid number (which prevents single
  //    minus sign)
  bool test2 = (!textX.isEmpty() || !textY.isEmpty());

  int posX, posY;
  bool test3 = true;
  if (!textX.isEmpty()) {
    test3 &= (m_validatorGraphX->validate(textX, posX) == QValidator::Acceptable);
  }
  if (!textY.isEmpty()) {
    test3 &= (m_validatorGraphY->validate(textY, posY) == QValidator::Acceptable);
  }

  m_btnOk->setEnabled (m_changed && test2 && test3);
}
