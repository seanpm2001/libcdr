/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* libcdr
 * Version: MPL 1.1 / GPLv2+ / LGPLv2+
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License or as specified alternatively below. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * Major Contributor(s):
 * Copyright (C) 2012 Fridrich Strba <fridrich.strba@bluewin.ch>
 *
 *
 * All Rights Reserved.
 *
 * For minor contributions see the git repository.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPLv2+"), or
 * the GNU Lesser General Public License Version 2 or later (the "LGPLv2+"),
 * in which case the provisions of the GPLv2+ or the LGPLv2+ are applicable
 * instead of those above.
 */

#include <math.h>
#include <string.h>
#include "CDRSVGGenerator.h"
#include "CDRContentCollector.h"
#include "CDRInternalStream.h"
#include "CMXDocument.h"
#include "libcdr_utils.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef DUMP_PATTERN
#define DUMP_PATTERN 0
#endif

#ifndef DUMP_VECT
#define DUMP_VECT 0
#endif

libcdr::CDRContentCollector::CDRContentCollector(libcdr::CDRParserState &ps, libwpg::WPGPaintInterface *painter) :
  m_painter(painter),
  m_isPageProperties(false), m_isPageStarted(false), m_ignorePage(false),
  m_page(ps.m_pages[0]), m_pageIndex(0), m_currentFildId(0), m_currentOutlId(0), m_spnd(0),
  m_currentObjectLevel(0), m_currentGroupLevel(0), m_currentVectLevel(0), m_currentPageLevel(0),
  m_currentImage(), m_currentText(0), m_currentBBox(), m_currentTextBox(), m_currentPath(),
  m_currentTransforms(), m_fillTransforms(), m_polygon(0), m_isInPolygon(false), m_isInSpline(false),
  m_outputElements(0), m_contentOutputElements(), m_fillOutputElements(),
  m_groupLevels(), m_groupTransforms(), m_splineData(), m_fillOpacity(1.0), m_ps(ps)
{
  m_outputElements = &m_contentOutputElements;
}

libcdr::CDRContentCollector::~CDRContentCollector()
{
  if (m_isPageStarted)
    _endPage();
}

void libcdr::CDRContentCollector::_startPage(double width, double height)
{
  if (m_ignorePage)
    return;
  WPXPropertyList propList;
  propList.insert("svg:width", width);
  propList.insert("svg:height", height);
  if (m_painter)
  {
    m_painter->startGraphics(propList);
    m_isPageStarted = true;
  }
}

void libcdr::CDRContentCollector::_endPage()
{
  if (!m_isPageStarted)
    return;
  while (!m_contentOutputElements.empty())
  {
    m_contentOutputElements.top().draw(m_painter);
    m_contentOutputElements.pop();
  }
  if (m_painter)
    m_painter->endGraphics();
  m_isPageStarted = false;
}

void libcdr::CDRContentCollector::collectPage(unsigned level)
{
  m_isPageProperties = true;
  m_ignorePage = false;
  m_currentPageLevel = level;
  m_page = m_ps.m_pages[m_pageIndex++];

}

void libcdr::CDRContentCollector::collectObject(unsigned level)
{
  if (!m_isPageStarted && !m_currentVectLevel && !m_ignorePage)
    _startPage(m_page.width, m_page.height);
  m_currentObjectLevel = level;
  m_currentFildId = 0;
  m_currentOutlId = 0;
  m_currentBBox = CDRBox();
}

void libcdr::CDRContentCollector::collectGroup(unsigned level)
{
  if (!m_isPageStarted && !m_currentVectLevel && !m_ignorePage)
    _startPage(m_page.width, m_page.height);
  WPXPropertyList propList;
  CDROutputElementList outputElement;
  // Since the CDR objects are drawn in reverse order, reverse the logic of groups too
  outputElement.addEndGroup();
  m_outputElements->push(outputElement);
  m_groupLevels.push(level);
  m_groupTransforms.push(CDRTransforms());
}

void libcdr::CDRContentCollector::collectVect(unsigned level)
{
  m_currentVectLevel = level;
  m_outputElements = &m_fillOutputElements;
  m_page.width = 0.0;
  m_page.height = 0.0;
  m_page.offsetX = 0.0;
  m_page.offsetY = 0.0;
}

void libcdr::CDRContentCollector::collectFlags(unsigned flags, bool considerFlags)
{
  if (m_isPageProperties && !(flags & 0x00ff0000))
  {
    if (!m_isPageStarted)
      _startPage(m_page.width, m_page.height);
  }
  else if (m_isPageProperties && considerFlags)
    m_ignorePage = true;
  m_isPageProperties = false;
}

void libcdr::CDRContentCollector::collectOtherList()
{
//  m_isPageProperties = false;
}

void libcdr::CDRContentCollector::collectCubicBezier(double x1, double y1, double x2, double y2, double x, double y)
{
  CDR_DEBUG_MSG(("CDRContentCollector::collectCubicBezier(%f, %f, %f, %f, %f, %f)\n", x1, y1, x2, y2, x, y));
  m_currentPath.appendCubicBezierTo(x1, y1, x2, y2, x, y);
}

void libcdr::CDRContentCollector::collectQuadraticBezier(double x1, double y1, double x, double y)
{
  CDR_DEBUG_MSG(("CDRContentCollector::collectQuadraticBezier(%f, %f, %f, %f)\n", x1, y1, x, y));
  m_currentPath.appendQuadraticBezierTo(x1, y1, x, y);
}

void libcdr::CDRContentCollector::collectMoveTo(double x, double y)
{
  CDR_DEBUG_MSG(("CDRContentCollector::collectMoveTo(%f, %f)\n", x, y));
  m_currentPath.appendMoveTo(x,y);
}

void libcdr::CDRContentCollector::collectLineTo(double x, double y)
{
  CDR_DEBUG_MSG(("CDRContentCollector::collectLineTo(%f, %f)\n", x, y));
  m_currentPath.appendLineTo(x, y);
}

void libcdr::CDRContentCollector::collectArcTo(double rx, double ry, bool largeArc, bool sweep, double x, double y)
{
  CDR_DEBUG_MSG(("CDRContentCollector::collectArcTo(%f, %f)\n", x, y));
  m_currentPath.appendArcTo(rx, ry, 0.0, largeArc, sweep, x, y);
}

void libcdr::CDRContentCollector::collectClosePath()
{
  CDR_DEBUG_MSG(("CDRContentCollector::collectClosePath\n"));
  m_currentPath.appendClosePath();
}

void libcdr::CDRContentCollector::_flushCurrentPath()
{
  CDR_DEBUG_MSG(("CDRContentCollector::collectFlushPath\n"));
  CDROutputElementList outputElement;
  if (!m_currentPath.empty() || (!m_splineData.empty() && m_isInSpline))
  {
    if (m_polygon && m_isInPolygon)
      m_polygon->create(m_currentPath);
    if (m_polygon)
    {
      delete m_polygon;
      m_polygon = 0;
    }
    m_isInPolygon = false;
    if (!m_splineData.empty() && m_isInSpline)
      m_splineData.create(m_currentPath);
    m_splineData.clear();
    m_isInSpline = false;
    bool firstPoint = true;
    bool wasMove = false;
    double initialX = 0.0;
    double initialY = 0.0;
    double previousX = 0.0;
    double previousY = 0.0;
    double x = 0.0;
    double y = 0.0;
    WPXPropertyList style;
    WPXPropertyListVector gradient;
    _fillProperties(style, gradient);
    _lineProperties(style);
    outputElement.addStyle(style, gradient);
    m_currentPath.transform(m_currentTransforms);
    if (!m_groupTransforms.empty())
      m_currentPath.transform(m_groupTransforms.top());
    CDRTransform tmpTrafo(1.0, 0.0, -m_page.offsetX, 0.0, 1.0, -m_page.offsetY);
    m_currentPath.transform(tmpTrafo);
    tmpTrafo = CDRTransform(1.0, 0.0, 0.0, 0.0, -1.0, m_page.height);
    m_currentPath.transform(tmpTrafo);

    std::vector<WPXPropertyList> tmpPath;

    WPXPropertyListVector path;
    m_currentPath.writeOut(path);

    bool isPathClosed = m_currentPath.isClosed();

    WPXPropertyListVector::Iter i(path);
    for (i.rewind(); i.next();)
    {
      if (!i()["libwpg:path-action"])
        continue;
      if (i()["svg:x"] && i()["svg:y"])
      {
        bool ignoreM = false;
        x = i()["svg:x"]->getDouble();
        y = i()["svg:y"]->getDouble();
        if (firstPoint)
        {
          initialX = x;
          initialY = y;
          firstPoint = false;
          wasMove = true;
        }
        else if (i()["libwpg:path-action"]->getStr() == "M")
        {
          // This is needed for a good generation of path from polygon
          if (CDR_ALMOST_ZERO(previousX - x) && CDR_ALMOST_ZERO(previousY - y))
            ignoreM = true;
          else
          {
            if (!tmpPath.empty())
            {
              if (!wasMove)
              {
                if ((CDR_ALMOST_ZERO(initialX - previousX) && CDR_ALMOST_ZERO(initialY - previousY)) || isPathClosed)
                {
                  WPXPropertyList node;
                  node.insert("libwpg:path-action", "Z");
                  tmpPath.push_back(node);
                }
              }
              else
              {
                tmpPath.pop_back();
              }
            }
          }

          if (!ignoreM)
          {
            initialX = x;
            initialY = y;
            wasMove = true;
          }

        }
        else
          wasMove = false;

        if (!ignoreM)
        {
          tmpPath.push_back(i());
          previousX = x;
          previousY = y;
        }

      }
    }
    if (!tmpPath.empty())
    {
      if (!wasMove)
      {
        if ((CDR_ALMOST_ZERO(initialX - previousX) && CDR_ALMOST_ZERO(initialY - previousY)) || isPathClosed)
        {
          WPXPropertyList closedPath;
          closedPath.insert("libwpg:path-action", "Z");
          tmpPath.push_back(closedPath);
        }
      }
      else
        tmpPath.pop_back();
    }
    if (!tmpPath.empty())
    {
      WPXPropertyListVector outputPath;
      for (std::vector<WPXPropertyList>::const_iterator iter = tmpPath.begin(); iter != tmpPath.end(); ++iter)
        outputPath.append(*iter);

      outputElement.addPath(outputPath);

    }
    m_currentPath.clear();
  }

  if (m_currentImage.getImage().size())
  {
    double cx = m_currentImage.getMiddleX();
    double cy = m_currentImage.getMiddleY();
    double corner1x = m_currentImage.m_x1;
    double corner1y = m_currentImage.m_y1;
    double corner2x = m_currentImage.m_x1;
    double corner2y = m_currentImage.m_y2;
    double corner3x = m_currentImage.m_x2;
    double corner3y = m_currentImage.m_y2;
    m_currentTransforms.applyToPoint(cx, cy);
    m_currentTransforms.applyToPoint(corner1x, corner1y);
    m_currentTransforms.applyToPoint(corner2x, corner2y);
    m_currentTransforms.applyToPoint(corner3x, corner3y);
    if (!m_groupTransforms.empty())
    {
      m_groupTransforms.top().applyToPoint(cx, cy);
      m_groupTransforms.top().applyToPoint(corner1x, corner1y);
      m_groupTransforms.top().applyToPoint(corner2x, corner2y);
      m_groupTransforms.top().applyToPoint(corner3x, corner3y);
    }
    CDRTransform tmpTrafo(1.0, 0.0, -m_page.offsetX, 0.0, 1.0, -m_page.offsetY);
    tmpTrafo.applyToPoint(cx, cy);
    tmpTrafo.applyToPoint(corner1x, corner1y);
    tmpTrafo.applyToPoint(corner2x, corner2y);
    tmpTrafo.applyToPoint(corner3x, corner3y);
    tmpTrafo = CDRTransform(1.0, 0.0, 0.0, 0.0, -1.0, m_page.height);
    tmpTrafo.applyToPoint(cx, cy);
    tmpTrafo.applyToPoint(corner1x, corner1y);
    tmpTrafo.applyToPoint(corner2x, corner2y);
    tmpTrafo.applyToPoint(corner3x, corner3y);
    bool flipX(m_currentTransforms.getFlipX());
    bool flipY(m_currentTransforms.getFlipY());
    double width = sqrt((corner2x - corner3x)*(corner2x - corner3x) + (corner2y - corner3y)*(corner2y - corner3y));
    double height = sqrt((corner2x - corner1x)*(corner2x - corner1x) + (corner2y - corner1y)*(corner2y - corner1y));
    double rotate = atan2(corner3y-corner2y, corner3x-corner2x);

    WPXPropertyList propList;

    propList.insert("svg:x", cx - width / 2.0);
    propList.insert("svg:width", width);
    propList.insert("svg:y", cy - height / 2.0);
    propList.insert("svg:height", height);

    if (flipX)
    {
      propList.insert("draw:mirror-horizontal", true);
      rotate = M_PI - rotate;
    }
    if (flipY)
    {
      propList.insert("draw:mirror-vertical", true);
      rotate *= -1.0;
    }

    while (rotate < 0.0)
      rotate += 2.0*M_PI;
    while (rotate > 2.0*M_PI)
      rotate -= 2.0*M_PI;

    if (rotate != 0.0)
      propList.insert("libwpg:rotate", rotate * 180 / M_PI, WPX_GENERIC);

    propList.insert("libwpg:mime-type", "image/bmp");

    outputElement.addGraphicObject(propList, m_currentImage.getImage());
  }
  if (m_currentText && !m_currentText->empty())
  {
    double x1 = m_currentTextBox.m_x;
    double y1 = m_currentTextBox.m_y;
    double x2 = m_currentTextBox.m_x + m_currentTextBox.m_w;
    double y2 = m_currentTextBox.m_y - m_currentTextBox.m_h;
    if (m_currentTextBox.m_h != 0.0 && m_currentTextBox.m_w != 0)
    {
      m_currentTransforms.applyToPoint(x1, y1);
      m_currentTransforms.applyToPoint(x2, y2);
      if (!m_groupTransforms.empty())
      {
        m_groupTransforms.top().applyToPoint(x1, y1);
        m_groupTransforms.top().applyToPoint(x2, y2);
      }
    }
    else if (m_currentBBox.getWidth() != 0.0 && m_currentBBox.getHeight() != 0.0)
    {
      x1 = m_currentBBox.getMinX();
      y1 = m_currentBBox.getMinY();
      x2 = m_currentBBox.getMinX() + m_currentBBox.getWidth();
      y2 = m_currentBBox.getMinY() + m_currentBBox.getHeight();
    }

    CDRTransform tmpTrafo(1.0, 0.0, -m_page.offsetX, 0.0, 1.0, -m_page.offsetY);
    tmpTrafo.applyToPoint(x1, y1);
    tmpTrafo.applyToPoint(x2, y2);
    tmpTrafo = CDRTransform(1.0, 0.0, 0.0, 0.0, -1.0, m_page.height);
    tmpTrafo.applyToPoint(x1, y1);
    tmpTrafo.applyToPoint(x2, y2);
    if (x1 > x2)
      std::swap(x1, x2);
    if (y1 > y2)
      std::swap(y1, y2);

    WPXPropertyList textFrameProps;
    textFrameProps.insert("svg:width", fabs(x2-x1));
    textFrameProps.insert("svg:height", fabs(y2-y1));
    textFrameProps.insert("svg:x", x1);
    textFrameProps.insert("svg:y", y1);
    textFrameProps.insert("fo:padding-top", 0.0);
    textFrameProps.insert("fo:padding-bottom", 0.0);
    textFrameProps.insert("fo:padding-left", 0.0);
    textFrameProps.insert("fo:padding-right", 0.0);
    outputElement.addStartTextObject(textFrameProps, WPXPropertyListVector());
    for (unsigned i = 0; i < m_currentText->size(); ++i)
    {
      WPXPropertyList paraProps;
      bool rtl = false;
      switch ((*m_currentText)[i].m_charStyle.m_align)
      {
      case 1:  // Left
        if (!rtl)
          paraProps.insert("fo:text-align", "left");
        else
          paraProps.insert("fo:text-align", "end");
        break;
      case 2:  // Center
        paraProps.insert("fo:text-align", "center");
        break;
      case 3:  // Right
        if (!rtl)
          paraProps.insert("fo:text-align", "end");
        else
          paraProps.insert("fo:text-align", "left");
        break;
      case 4:  // Full justify
        paraProps.insert("fo:text-align", "justify");
        break;
      case 5:  // Force justify
        paraProps.insert("fo:text-align", "full");
        break;
      case 0:  // None
      default:
        break;
      }
      paraProps.insert("fo:text-indent", (*m_currentText)[i].m_charStyle.m_firstIndent);
      paraProps.insert("fo:margin-left", (*m_currentText)[i].m_charStyle.m_leftIndent);
      paraProps.insert("fo:margin-right", (*m_currentText)[i].m_charStyle.m_rightIndent);
      outputElement.addStartTextLine(paraProps);
      WPXPropertyList spanProps;
      double fontSize = (double)cdr_round(144.0*(*m_currentText)[i].m_charStyle.m_fontSize) / 2.0;
      spanProps.insert("fo:font-size", fontSize, WPX_POINT);
      std::map<unsigned, CDRFont>::const_iterator iterFont = m_ps.m_fonts.find((*m_currentText)[i].m_charStyle.m_fontId);
      if (iterFont != m_ps.m_fonts.end())
        spanProps.insert("style:font-name", iterFont->second.m_name);
      outputElement.addStartTextSpan(spanProps);
      outputElement.addInsertText((*m_currentText)[i].m_text);
      outputElement.addEndTextSpan();
      outputElement.addEndTextLine();
    }
    outputElement.addEndTextObject();
  }
  m_currentImage = libcdr::CDRImage();
  if (!outputElement.empty())
    m_outputElements->push(outputElement);
  m_currentTransforms.clear();
  m_fillTransforms = libcdr::CDRTransforms();
  m_fillOpacity = 1.0;
  m_currentText = 0;
}

void libcdr::CDRContentCollector::collectTransform(const CDRTransforms &transforms, bool considerGroupTransform)
{
  if (m_currentObjectLevel)
    m_currentTransforms = transforms;
  else if (!m_groupLevels.empty() && considerGroupTransform)
    m_groupTransforms.top() = transforms;
}

void libcdr::CDRContentCollector::collectFillTransform(const CDRTransforms &fillTrafos)
{
  m_fillTransforms = fillTrafos;
}

void libcdr::CDRContentCollector::collectLevel(unsigned level)
{
  if (level <= m_currentObjectLevel)
  {
    _flushCurrentPath();
    m_currentObjectLevel = 0;
  }
  while (!m_groupLevels.empty() && level <= m_groupLevels.top())
  {
    WPXPropertyList propList;
    CDROutputElementList outputElement;
    // since the CDR objects are drawn in reverse order, reverse group marks too
    outputElement.addStartGroup(propList);
    m_outputElements->push(outputElement);
    m_groupLevels.pop();
    m_groupTransforms.pop();
  }
  if (m_currentVectLevel && m_spnd && m_groupLevels.empty() && !m_fillOutputElements.empty())
  {
    CDRStringVector svgOutput;
    CDRSVGGenerator generator(svgOutput);
    WPXPropertyList propList;
    propList.insert("svg:width", m_page.width);
    propList.insert("svg:height", m_page.height);
    generator.startGraphics(propList);
    while (!m_fillOutputElements.empty())
    {
      m_fillOutputElements.top().draw(&generator);
      m_fillOutputElements.pop();
    }
    generator.endGraphics();
    if (!svgOutput.empty())
    {
      const char *header =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n";
      WPXBinaryData output((const unsigned char *)header, strlen(header));
      output.append((unsigned char *)svgOutput[0].cstr(), strlen(svgOutput[0].cstr()));
      m_ps.m_vects[m_spnd] = output;
    }
#if DUMP_VECT
    WPXString filename;
    filename.sprintf("vect%.8x.svg", m_spnd);
    FILE *f = fopen(filename.cstr(), "wb");
    if (f)
    {
      const unsigned char *tmpBuffer = m_ps.m_vects[m_spnd].getDataBuffer();
      for (unsigned long k = 0; k < m_ps.m_vects[m_spnd].size(); k++)
        fprintf(f, "%c",tmpBuffer[k]);
      fclose(f);
    }
#endif
    m_spnd = 0;
    m_page.width = 0.0;
    m_page.height = 0.0;
    m_page.offsetX = 0.0;
    m_page.offsetY = 0.0;
  }
  if (level <= m_currentVectLevel)
  {
    m_currentVectLevel = 0;
    m_outputElements = &m_contentOutputElements;
    m_page = m_ps.m_pages[m_pageIndex ? m_pageIndex-1 : 0];
  }
  if (level <= m_currentPageLevel)
  {
    _endPage();
    m_currentPageLevel = 0;
  }
}

void libcdr::CDRContentCollector::collectFildId(unsigned id)
{
  m_currentFildId = id;
}

void libcdr::CDRContentCollector::collectOutlId(unsigned id)
{
  m_currentOutlId = id;
}

void libcdr::CDRContentCollector::collectRotate(double angle, double cx, double cy)
{
  CDRTransform trafo1(1.0, 0.0, -cx, 0.0, 1.0, -cy);
  m_currentPath.transform(trafo1);
  CDRTransform trafo2(cos(angle), -sin(angle), 0, sin(angle), cos(angle), 0);
  m_currentPath.transform(trafo2);
  CDRTransform trafo3(1.0, 0.0, cx, 0.0, 1.0, cy);
  m_currentPath.transform(trafo3);
}

void libcdr::CDRContentCollector::collectPolygon()
{
  m_isInPolygon = true;
}

void libcdr::CDRContentCollector::collectSpline()
{
  m_isInSpline = true;
}

void libcdr::CDRContentCollector::collectPolygonTransform(unsigned numAngles, unsigned nextPoint, double rx, double ry, double cx, double cy)
{
  if (m_polygon)
    delete m_polygon;
  m_polygon = new CDRPolygon(numAngles, nextPoint, rx, ry, cx, cy);
}

void libcdr::CDRContentCollector::_fillProperties(WPXPropertyList &propList, WPXPropertyListVector &vec)
{
  if (m_fillOpacity < 1.0)
    propList.insert("draw:opacity", m_fillOpacity, WPX_PERCENT);
  if (m_currentFildId == 0)
    propList.insert("draw:fill", "none");
  else
  {
    std::map<unsigned, CDRFillStyle>::iterator iter = m_ps.m_fillStyles.find(m_currentFildId);
    if (iter == m_ps.m_fillStyles.end())
      propList.insert("draw:fill", "none");
    else
    {
      switch (iter->second.fillType)
      {
      case 1: // Solid
        propList.insert("draw:fill", "solid");
        propList.insert("draw:fill-color", m_ps.getRGBColorString(iter->second.color1));
        propList.insert("svg:fill-rule", "evenodd");
        break;
      case 2: // Gradient
        if (iter->second.gradient.m_stops.empty())
          propList.insert("draw:fill", "none");
        else if (iter->second.gradient.m_stops.size() == 1)
        {
          propList.insert("draw:fill", "solid");
          propList.insert("draw:fill-color", m_ps.getRGBColorString(iter->second.gradient.m_stops[0].m_color));
          propList.insert("svg:fill-rule", "evenodd");
        }
        else if (iter->second.gradient.m_stops.size() == 2)
        {
          double angle = iter->second.gradient.m_angle;
          while (angle < 0.0)
            angle += 360.0;
          while (angle > 360.0)
            angle -= 360.0;
          propList.insert("draw:fill", "gradient");
          propList.insert("draw:start-color", m_ps.getRGBColorString(iter->second.gradient.m_stops[0].m_color));
          propList.insert("draw:end-color", m_ps.getRGBColorString(iter->second.gradient.m_stops[1].m_color));
          propList.insert("draw:angle", (int)angle);
          switch (iter->second.gradient.m_type)
          {
          case 1: // linear
          case 3: // conical
            propList.insert("draw:style", "linear");
            angle += 90.0;
            while (angle < 0.0)
              angle += 360.0;
            while (angle > 360.0)
              angle -= 360.0;
            propList.insert("draw:angle", (int)angle);
            propList.insert("draw:border", (double)(iter->second.gradient.m_edgeOffset)/100.0, WPX_PERCENT);
            break;
          case 2: // radial
            propList.insert("draw:border", (2.0 * (double)(iter->second.gradient.m_edgeOffset)/100.0), WPX_PERCENT);
            propList.insert("draw:style", "radial");
            propList.insert("svg:cx", (double)(0.5 + iter->second.gradient.m_centerXOffset/200.0), WPX_PERCENT);
            propList.insert("svg:cy", (double)(0.5 + iter->second.gradient.m_centerXOffset/200.0), WPX_PERCENT);
            break;
          case 4: // square
            propList.insert("draw:border", (2.0 * (double)(iter->second.gradient.m_edgeOffset)/100.0), WPX_PERCENT);
            propList.insert("draw:style", "square");
            propList.insert("svg:cx", (double)(0.5 + iter->second.gradient.m_centerXOffset/200.0), WPX_PERCENT);
            propList.insert("svg:cy", (double)(0.5 + iter->second.gradient.m_centerXOffset/200.0), WPX_PERCENT);
            break;
          default:
            propList.insert("draw:style", "linear");
            angle += 90.0;
            while (angle < 0.0)
              angle += 360.0;
            while (angle > 360.0)
              angle -= 360.0;
            propList.insert("draw:angle", (int)angle);
            for (unsigned i = 0; i < iter->second.gradient.m_stops.size(); i++)
            {
              libcdr::CDRGradientStop &gradStop = iter->second.gradient.m_stops[i];
              WPXPropertyList stopElement;
              stopElement.insert("svg:offset", gradStop.m_offset, WPX_PERCENT);
              stopElement.insert("svg:stop-color", m_ps.getRGBColorString(gradStop.m_color));
              stopElement.insert("svg:stop-opacity", m_fillOpacity, WPX_PERCENT);
              vec.append(stopElement);
            }
            break;
          }
        }
        else // output svg gradient as a hail mary pass towards ODG that does not really support it
        {
          propList.insert("draw:fill", "gradient");
          propList.insert("draw:style", "linear");
          double angle = iter->second.gradient.m_angle;
          angle += 90.0;
          while (angle < 0.0)
            angle += 360.0;
          while (angle > 360.0)
            angle -= 360.0;
          propList.insert("draw:angle", (int)angle);
          for (unsigned i = 0; i < iter->second.gradient.m_stops.size(); i++)
          {
            libcdr::CDRGradientStop &gradStop = iter->second.gradient.m_stops[i];
            WPXPropertyList stopElement;
            stopElement.insert("svg:offset", gradStop.m_offset, WPX_PERCENT);
            stopElement.insert("svg:stop-color", m_ps.getRGBColorString(gradStop.m_color));
            stopElement.insert("svg:stop-opacity", m_fillOpacity, WPX_PERCENT);
            vec.append(stopElement);
          }
        }
        break;
      case 7: // Pattern
      {
        std::map<unsigned, CDRPattern>::iterator iterPattern = m_ps.m_patterns.find(iter->second.imageFill.id);
        if (iterPattern != m_ps.m_patterns.end())
        {
          propList.insert("draw:fill", "bitmap");
          WPXBinaryData image;
          _generateBitmapFromPattern(image, iterPattern->second, iter->second.color1, iter->second.color2);
#if DUMP_PATTERN
          WPXString filename;
          filename.sprintf("pattern%.8x.bmp", iter->second.imageFill.id);
          FILE *f = fopen(filename.cstr(), "wb");
          if (f)
          {
            const unsigned char *tmpBuffer = image.getDataBuffer();
            for (unsigned long k = 0; k < image.size(); k++)
              fprintf(f, "%c",tmpBuffer[k]);
            fclose(f);
          }
#endif
          propList.insert("draw:fill-image", image.getBase64Data());
          propList.insert("libwpg:mime-type", "image/bmp");
          propList.insert("style:repeat", "repeat");
          if (iter->second.imageFill.isRelative)
          {
            propList.insert("svg:width", iter->second.imageFill.width, WPX_PERCENT);
            propList.insert("svg:height", iter->second.imageFill.height, WPX_PERCENT);
          }
          else
          {
            double scaleX = 1.0;
            double scaleY = 1.0;
            if (iter->second.imageFill.flags & 0x04) // scale fill with image
            {
              scaleX = m_currentTransforms.getScaleX();
              scaleY = m_currentTransforms.getScaleY();
            }
            propList.insert("svg:width", iter->second.imageFill.width * scaleX);
            propList.insert("svg:height", iter->second.imageFill.height * scaleY);
          }
          propList.insert("draw:fill-image-ref-point", "bottom-left");
          if (iter->second.imageFill.isRelative)
          {
            if (iter->second.imageFill.xOffset != 0.0 && iter->second.imageFill.xOffset != 1.0)
              propList.insert("draw:fill-image-ref-point-x", iter->second.imageFill.xOffset, WPX_PERCENT);
            if (iter->second.imageFill.yOffset != 0.0 && iter->second.imageFill.yOffset != 1.0)
              propList.insert("draw:fill-image-ref-point-y", iter->second.imageFill.yOffset, WPX_PERCENT);
          }
          else
          {
            if (m_fillTransforms.getTranslateX() != 0.0)
            {
              double xOffset = m_fillTransforms.getTranslateX() / iter->second.imageFill.width;
              while (xOffset < 0.0)
                xOffset += 1.0;
              while (xOffset > 1.0)
                xOffset -= 1.0;
              propList.insert("draw:fill-image-ref-point-x", xOffset, WPX_PERCENT);
            }
            if (m_fillTransforms.getTranslateY() != 0.0)
            {
              double yOffset = m_fillTransforms.getTranslateY() / iter->second.imageFill.width;
              while (yOffset < 0.0)
                yOffset += 1.0;
              while (yOffset > 1.0)
                yOffset -= 1.0;
              propList.insert("draw:fill-image-ref-point-y", 1.0 - yOffset, WPX_PERCENT);
            }
          }
        }
        else
        {
          // We did not find the pattern, so fill solid with the background colour
          propList.insert("draw:fill", "solid");
          propList.insert("draw:fill-color", m_ps.getRGBColorString(iter->second.color2));
          propList.insert("svg:fill-rule", "evenodd");
        }
      }
      break;
      case 9: // Bitmap
      case 11: // Texture
      {
        std::map<unsigned, WPXBinaryData>::iterator iterBmp = m_ps.m_bmps.find(iter->second.imageFill.id);
        if (iterBmp != m_ps.m_bmps.end())
        {
          propList.insert("libwpg:mime-type", "image/bmp");
          propList.insert("draw:fill", "bitmap");
          propList.insert("draw:fill-image", iterBmp->second.getBase64Data());
          propList.insert("style:repeat", "repeat");
          if (iter->second.imageFill.isRelative)
          {
            propList.insert("svg:width", iter->second.imageFill.width, WPX_PERCENT);
            propList.insert("svg:height", iter->second.imageFill.height, WPX_PERCENT);
          }
          else
          {
            double scaleX = 1.0;
            double scaleY = 1.0;
            if (iter->second.imageFill.flags & 0x04) // scale fill with image
            {
              scaleX = m_currentTransforms.getScaleX();
              scaleY = m_currentTransforms.getScaleY();
            }
            propList.insert("svg:width", iter->second.imageFill.width * scaleX);
            propList.insert("svg:height", iter->second.imageFill.height * scaleY);
          }
          propList.insert("draw:fill-image-ref-point", "bottom-left");
          if (iter->second.imageFill.isRelative)
          {
            if (iter->second.imageFill.xOffset != 0.0 && iter->second.imageFill.xOffset != 1.0)
              propList.insert("draw:fill-image-ref-point-x", iter->second.imageFill.xOffset, WPX_PERCENT);
            if (iter->second.imageFill.yOffset != 0.0 && iter->second.imageFill.yOffset != 1.0)
              propList.insert("draw:fill-image-ref-point-y", iter->second.imageFill.yOffset, WPX_PERCENT);
          }
          else
          {
            if (m_fillTransforms.getTranslateX() != 0.0)
            {
              double xOffset = m_fillTransforms.getTranslateX() / iter->second.imageFill.width;
              while (xOffset < 0.0)
                xOffset += 1.0;
              while (xOffset > 1.0)
                xOffset -= 1.0;
              propList.insert("draw:fill-image-ref-point-x", xOffset, WPX_PERCENT);
            }
            if (m_fillTransforms.getTranslateY() != 0.0)
            {
              double yOffset = m_fillTransforms.getTranslateY() / iter->second.imageFill.width;
              while (yOffset < 0.0)
                yOffset += 1.0;
              while (yOffset > 1.0)
                yOffset -= 1.0;
              propList.insert("draw:fill-image-ref-point-y", 1.0 - yOffset, WPX_PERCENT);
            }
          }
        }
        else
          propList.insert("draw:fill", "none");
      }
      break;
      case 10: // Full color
      {
        std::map<unsigned, WPXBinaryData>::iterator iterVect = m_ps.m_vects.find(iter->second.imageFill.id);
        if (iterVect != m_ps.m_vects.end())
        {
          propList.insert("draw:fill", "bitmap");
          propList.insert("libwpg:mime-type", "image/svg+xml");
          propList.insert("draw:fill-image", iterVect->second.getBase64Data());
          propList.insert("style:repeat", "repeat");
          if (iter->second.imageFill.isRelative)
          {
            propList.insert("svg:width", iter->second.imageFill.width, WPX_PERCENT);
            propList.insert("svg:height", iter->second.imageFill.height, WPX_PERCENT);
          }
          else
          {
            double scaleX = 1.0;
            double scaleY = 1.0;
            if (iter->second.imageFill.flags & 0x04) // scale fill with image
            {
              scaleX = m_currentTransforms.getScaleX();
              scaleY = m_currentTransforms.getScaleY();
            }
            propList.insert("svg:width", iter->second.imageFill.width * scaleX);
            propList.insert("svg:height", iter->second.imageFill.height * scaleY);
          }
          propList.insert("draw:fill-image-ref-point", "bottom-left");
          if (iter->second.imageFill.isRelative)
          {
            if (iter->second.imageFill.xOffset != 0.0 && iter->second.imageFill.xOffset != 1.0)
              propList.insert("draw:fill-image-ref-point-x", iter->second.imageFill.xOffset, WPX_PERCENT);
            if (iter->second.imageFill.yOffset != 0.0 && iter->second.imageFill.yOffset != 1.0)
              propList.insert("draw:fill-image-ref-point-y", iter->second.imageFill.yOffset, WPX_PERCENT);
          }
          else
          {
            if (m_fillTransforms.getTranslateX() != 0.0)
            {
              double xOffset = m_fillTransforms.getTranslateX() / iter->second.imageFill.width;
              while (xOffset < 0.0)
                xOffset += 1.0;
              while (xOffset > 1.0)
                xOffset -= 1.0;
              propList.insert("draw:fill-image-ref-point-x", xOffset, WPX_PERCENT);
            }
            if (m_fillTransforms.getTranslateY() != 0.0)
            {
              double yOffset = m_fillTransforms.getTranslateY() / iter->second.imageFill.width;
              while (yOffset < 0.0)
                yOffset += 1.0;
              while (yOffset > 1.0)
                yOffset -= 1.0;
              propList.insert("draw:fill-image-ref-point-y", 1.0 - yOffset, WPX_PERCENT);
            }
          }
        }
        else
          propList.insert("draw:fill", "none");
      }
      break;
      default:
        propList.insert("draw:fill", "none");
        break;
      }
    }
  }
}

void libcdr::CDRContentCollector::_lineProperties(WPXPropertyList &propList)
{
  if (m_currentOutlId == 0)
  {
    propList.insert("draw:stroke", "solid");
    propList.insert("svg:stroke-width", 0.0);
    propList.insert("svg:stroke-color", "#000000");
  }
  else
  {
    std::map<unsigned, CDRLineStyle>::iterator iter = m_ps.m_lineStyles.find(m_currentOutlId);
    if (iter == m_ps.m_lineStyles.end())
    {
      propList.insert("draw:stroke", "solid");
      propList.insert("svg:stroke-width", 0.0);
      propList.insert("svg:stroke-color", "#000000");
    }
    else if (iter->second.lineType & 0x1)
      propList.insert("draw:stroke", "none");
    else if (iter->second.lineType & 0x6)
    {
      if (iter->second.dashArray.size() && (iter->second.lineType & 0x4))
        propList.insert("draw:stroke", "dash");
      else
        propList.insert("draw:stroke", "solid");
      double scale = 1.0;
      if (iter->second.lineType & 0x20) // scale line with image
      {
        scale = m_currentTransforms.getScaleX();
        double scaleY = m_currentTransforms.getScaleY();
        if (scaleY > scale)
          scale = scaleY;
      }
      scale *= iter->second.stretch;
      propList.insert("svg:stroke-width", iter->second.lineWidth * scale);
      propList.insert("svg:stroke-color", m_ps.getRGBColorString(iter->second.color));

      switch (iter->second.capsType)
      {
      case 1:
        propList.insert("svg:stroke-linecap", "round");
        break;
      case 2:
        propList.insert("svg:stroke-linecap", "square");
        break;
      default:
        propList.insert("svg:stroke-linecap", "butt");
      }

      switch (iter->second.joinType)
      {
      case 1:
        propList.insert("svg:stroke-linejoin", "round");
        break;
      case 2:
        propList.insert("svg:stroke-linejoin", "bevel");
        break;
      default:
        propList.insert("svg:stroke-linejoin", "miter");
      }

      if (iter->second.dashArray.size())
      {
        int dots1 = 0;
        int dots2 = 0;
        unsigned dots1len = 0;
        unsigned dots2len = 0;
        unsigned gap = 0;

        if (iter->second.dashArray.size() >= 2)
        {
          dots1len = iter->second.dashArray[0];
          gap = iter->second.dashArray[1];
        }

        unsigned count = iter->second.dashArray.size() / 2;
        unsigned i = 0;
        for (; i < count;)
        {
          if (dots1len == iter->second.dashArray[2*i])
            dots1++;
          else
            break;
          gap = gap < iter->second.dashArray[2*i+1] ?  iter->second.dashArray[2*i+1] : gap;
          i++;
        }
        if (i < count)
        {
          dots2len = iter->second.dashArray[2*i];
          gap = gap < iter->second.dashArray[2*i+1] ? iter->second.dashArray[2*i+1] : gap;
        }
        for (; i < count;)
        {
          if (dots2len == iter->second.dashArray[2*i])
            dots2++;
          else
            break;
          gap = gap < iter->second.dashArray[2*i+1] ? iter->second.dashArray[2*i+1] : gap;
          i++;
        }
        if (!dots2)
        {
          dots2 = dots1;
          dots2len = dots1len;
        }
        propList.insert("draw:dots1", dots1);
        propList.insert("draw:dots1-length", 72.0*(iter->second.lineWidth * scale)*dots1len, WPX_POINT);
        propList.insert("draw:dots2", dots2);
        propList.insert("draw:dots2-length", 72.0*(iter->second.lineWidth * scale)*dots2len, WPX_POINT);
        propList.insert("draw:distance", 72.0*(iter->second.lineWidth * scale)*gap, WPX_POINT);
      }
    }
    else
    {
      propList.insert("draw:stroke", "solid");
      propList.insert("svg:stroke-width", 0.0);
      propList.insert("svg:stroke-color", "#000000");
    }

  }
}

void libcdr::CDRContentCollector::_generateBitmapFromPattern(WPXBinaryData &bitmap, const CDRPattern &pattern, const CDRColor &fgColor, const CDRColor &bgColor)
{
  unsigned height = pattern.height;
  unsigned width = pattern.width;
  unsigned tmpPixelSize = (unsigned)(height * width);
  if (tmpPixelSize < (unsigned)height) // overflow
    return;

  unsigned tmpDIBImageSize = tmpPixelSize * 4;
  if (tmpPixelSize > tmpDIBImageSize) // overflow !!!
    return;

  unsigned tmpDIBOffsetBits = 14 + 40;
  unsigned tmpDIBFileSize = tmpDIBOffsetBits + tmpDIBImageSize;
  if (tmpDIBImageSize > tmpDIBFileSize) // overflow !!!
    return;

  // Create DIB file header
  writeU16(bitmap, 0x4D42);  // Type
  writeU32(bitmap, tmpDIBFileSize); // Size
  writeU16(bitmap, 0); // Reserved1
  writeU16(bitmap, 0); // Reserved2
  writeU32(bitmap, tmpDIBOffsetBits); // OffsetBits

  // Create DIB Info header
  writeU32(bitmap, 40); // Size

  writeU32(bitmap, width);  // Width
  writeU32(bitmap, height); // Height

  writeU16(bitmap, 1); // Planes
  writeU16(bitmap, 32); // BitCount
  writeU32(bitmap, 0); // Compression
  writeU32(bitmap, tmpDIBImageSize); // SizeImage
  writeU32(bitmap, 0); // XPelsPerMeter
  writeU32(bitmap, 0); // YPelsPerMeter
  writeU32(bitmap, 0); // ColorsUsed
  writeU32(bitmap, 0); // ColorsImportant

  // The Bitmaps in CDR are padded to 32bit border
  unsigned lineWidth = (width + 7) / 8;

  unsigned foreground = m_ps._getRGBColor(fgColor);
  unsigned background = m_ps._getRGBColor(bgColor);

  for (unsigned j = height; j > 0; --j)
  {
    unsigned i = 0;
    unsigned k = 0;
    while (i <lineWidth && k < width)
    {
      unsigned l = 0;
      unsigned char c = pattern.pattern[(j-1)*lineWidth+i];
      i++;
      while (k < width && l < 8)
      {
        if (c & 0x80)
          writeU32(bitmap, background);
        else
          writeU32(bitmap, foreground);
        c <<= 1;
        l++;
        k++;
      }
    }
  }
}

void libcdr::CDRContentCollector::collectBitmap(unsigned imageId, double x1, double x2, double y1, double y2)
{
  std::map<unsigned, WPXBinaryData>::iterator iter = m_ps.m_bmps.find(imageId);
  if (iter != m_ps.m_bmps.end())
    m_currentImage = CDRImage(iter->second, x1, x2, y1, y2);
}

void libcdr::CDRContentCollector::collectPpdt(const std::vector<std::pair<double, double> > &points, const std::vector<unsigned> &knotVector)
{
  m_splineData = CDRSplineData(points, knotVector);
}

void libcdr::CDRContentCollector::collectFillOpacity(double opacity)
{
  m_fillOpacity = opacity;
}

void libcdr::CDRContentCollector::collectBBox(double x0, double y0, double x1, double y1)
{
  CDRBox bBox(x0, y0, x1, y1);
  if (m_currentVectLevel && m_page.width == 0.0 && m_page.height == 0.0)
  {
    m_page.width = bBox.getWidth();
    m_page.height = bBox.getHeight();
    m_page.offsetX = bBox.getMinX();
    m_page.offsetY = bBox.getMinY();
  }
  m_currentBBox = bBox;
}

void libcdr::CDRContentCollector::collectSpnd(unsigned spnd)
{
  if (m_currentVectLevel && !m_spnd)
    m_spnd = spnd;
  else if (!m_currentVectLevel)
    m_spnd = spnd;
}

void libcdr::CDRContentCollector::collectVectorPattern(unsigned id, const WPXBinaryData &data)
{
  WPXInputStream *input = const_cast<WPXInputStream *>(data.getDataStream());
  input->seek(0, WPX_SEEK_SET);
  if (!libcdr::CMXDocument::isSupported(input))
    return;
  CDRStringVector svgOutput;
  input->seek(0, WPX_SEEK_SET);
  if (!libcdr::CMXDocument::generateSVG(input, svgOutput))
    return;
  if (!svgOutput.empty())
  {
    const char *header =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n";
    WPXBinaryData output((const unsigned char *)header, strlen(header));
    output.append((unsigned char *)svgOutput[0].cstr(), strlen(svgOutput[0].cstr()));
    m_ps.m_vects[id] = output;
  }
#if DUMP_VECT
  WPXString filename;
  filename.sprintf("vect%.8x.svg", id);
  FILE *f = fopen(filename.cstr(), "wb");
  if (f)
  {
    const unsigned char *tmpBuffer = m_ps.m_vects[id].getDataBuffer();
    for (unsigned long k = 0; k < m_ps.m_vects[id].size(); k++)
      fprintf(f, "%c",tmpBuffer[k]);
    fclose(f);
  }
#endif
}

void libcdr::CDRContentCollector::collectArtisticText(double x, double y)
{
  m_currentTextBox = CDRBox(x, y, x, y);
  m_currentBBox.m_w *= 2.0;
  std::map<unsigned, std::vector<CDRText> >::const_iterator iter = m_ps.m_texts.find(m_spnd);
  if (iter != m_ps.m_texts.end())
    m_currentText = &(iter->second);
}

void libcdr::CDRContentCollector::collectParagraphText(double x, double y, double width, double height)
{
  m_currentTextBox.m_x = x;
  m_currentTextBox.m_y = y;
  m_currentTextBox.m_w = width;
  m_currentTextBox.m_h = height;
  std::map<unsigned, std::vector<CDRText> >::const_iterator iter = m_ps.m_texts.find(m_spnd);
  if (iter != m_ps.m_texts.end())
    m_currentText = &(iter->second);
}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
