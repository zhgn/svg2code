#include "codepaintdevice.h"
#include <QtDebug>

class MyPaintEngine : public QObject, public QPaintEngine
{
    Q_OBJECT

public:
    explicit MyPaintEngine(QObject *parent = 0, PaintEngineFeatures features = 0);

    bool begin(QPaintDevice *pdev);
    bool end();
    void updateState(const QPaintEngineState &state);
    void drawPath(const QPainterPath &path);
    void drawPixmap(const QRectF &r, const QPixmap &pm, const QRectF &sr);
    Type type() const;

signals:
    void stateUpdated(const QPaintEngineState &state);
    void pathDrawn(const QPainterPath &path);
};

MyPaintEngine::MyPaintEngine(QObject *parent, PaintEngineFeatures features)
    : QObject(parent)
    , QPaintEngine(features)
{
}

bool MyPaintEngine::begin(QPaintDevice *pdev)
{
    return true;
}

bool MyPaintEngine::end()
{
    return true;
}

void MyPaintEngine::updateState(const QPaintEngineState &state)
{
    emit stateUpdated(state);
}

void MyPaintEngine::drawPath(const QPainterPath &path)
{
    emit pathDrawn(path);
}

void MyPaintEngine::drawPixmap(const QRectF &r, const QPixmap &pm, const QRectF &sr)
{
}

QPaintEngine::Type MyPaintEngine::type() const
{
    return QPaintEngine::User;
}

CodePaintDevice::CodePaintDevice(const QString &prefix, QObject *parent, QPaintEngine::PaintEngineFeatures features)
    : QObject(parent)
    , QPaintDevice()
    , m_prefix(prefix + QLatin1Char('_'))
    , m_paintEngine(new MyPaintEngine(this, features))
    , m_pen(Qt::NoPen)
    , m_brush(Qt::NoBrush)
    , m_activePen(Qt::NoPen)
    , m_activeBrush(Qt::NoBrush)
{
    connect(m_paintEngine, SIGNAL(stateUpdated(QPaintEngineState)), SLOT(updateState(QPaintEngineState)));
    connect(m_paintEngine, SIGNAL(pathDrawn(QPainterPath)), SLOT(drawPath(QPainterPath)));
}

CodePaintDevice::~CodePaintDevice()
{
}

void CodePaintDevice::addElement(const Element &element)
{
    m_pen.setStyle(Qt::NoPen);
    m_brush.setStyle(Qt::NoBrush);
    m_activePen.setStyle(Qt::NoPen);
    m_activeBrush.setStyle(Qt::NoBrush);
    m_elements.append(element);
}

QPaintEngine *CodePaintDevice::paintEngine() const
{
    return m_paintEngine;
}

int CodePaintDevice::metric(PaintDeviceMetric m) const
{
    switch (m) {
    default: return 100;
    }
}

void CodePaintDevice::updateState(const QPaintEngineState &state)
{
    m_pen = state.pen();
    m_brush = state.brush();
}

void CodePaintDevice::drawPath(const QPainterPath &path)
{
    onDrawPath(path);
}

QString CodePaintDeviceQt::code() const
{
    return QString();
}

void CodePaintDeviceQt::onNewElement(const Element &element)
{
}

void CodePaintDeviceQt::onDrawPath(const QPainterPath &path)
{
}

CodePaintDeviceHTML5Canvas::CodePaintDeviceHTML5Canvas(const QString &prefix, QObject *parent)
    : CodePaintDevice(prefix, parent, QPaintEngine::PainterPaths)
{
}

QString CodePaintDeviceHTML5Canvas::code() const
{
    QString result = "// This file has been generated by svg2code\n";
    QString drawFunctions;
    const QString dictionaryName = m_prefix + "elements";
    QString dictionary = "\nvar " + dictionaryName +" = {\n    ";
    int count = 0;
    foreach (const Element &element, m_elements) {
        const QString functionName = m_prefix + QString::fromLatin1("draw_%1").arg(count, 3, 10, QLatin1Char('0'));
        drawFunctions.append("\n"
                             "function " + functionName + "(c) // '" + element.id + "'\n"
                             "{\n"
                             + element.code +
                             "}\n");
        if (count > 0)
            dictionary.append(",\n    ");
        dictionary.append("'" + element.id + "': { id: '" + element.id + "', bounds: {"
                          + " x: " + QString::number(element.rect.x(), 'f', 1)
                          + ", y: " + QString::number(element.rect.y(), 'f', 1)
                          + ", width: " + QString::number(element.rect.width(), 'f', 1)
                          + ", height: " + QString::number(element.rect.height(), 'f', 1)
                          + " }, drawfunction: " + functionName + " }");
        count++;
    }
    dictionary.append("\n}\n");
    result.append(drawFunctions);
    result.append(dictionary);
    result.append("\n"
                  "function " + m_prefix + "draw(context, id, x, y, width, height)\n"
                  "{\n"
                  "    var element = " + dictionaryName + "[id];\n"
                  "    if (element !== undefined) {\n"
                  "        context.save();\n"
                  "        context.translate(x, y);\n"
                  "        if (width !== undefined && height !== undefined)\n"
                  "            context.scale(width / element.bounds.width, height / element.bounds.height);\n"
                  "        context.translate(-element.bounds.x, -element.bounds.y);\n"
                  "        element.drawfunction(context);\n"
                  "        context.restore();\n"
                  "    }\n"
                  "}\n\n");
    return result;
}

void CodePaintDeviceHTML5Canvas::onNewElement(const Element &element)
{
    Q_UNUSED(element)
}

static QString toCssStyle(const QColor &color)
{
    QString rgb = QString::number(color.red()) + ", "
            + QString::number(color.green()) + ", "
            + QString::number(color.blue());
    return color.alpha() == 255 ? "rgb(" + rgb + ")"
                                : "rgba(" + rgb + ", " + QString::number(color.alphaF(), 'f', 1) + ")";
}

void CodePaintDeviceHTML5Canvas::onDrawPath(const QPainterPath &path)
{
    if (m_pen.style() == Qt::NoPen && m_brush.style() == Qt::NoBrush)
        return;
    QString &code = m_elements.last().code;
    code.append("    c.beginPath();\n");
    for (int i = 0; i < path.elementCount(); i++) {
        const QPainterPath::Element &element = path.elementAt(i);
        switch (element.type) {
            case QPainterPath::LineToElement:
                code.append("    c.lineTo(" + QString::number(element.x, 'f', 1) + ", " + QString::number(element.y, 'f', 1) + ");\n");
            break;
            case QPainterPath::CurveToElement: {
                code.append("    c.bezierCurveTo(" + QString::number(element.x, 'f', 1) + ", " + QString::number(element.y, 'f', 1)  + ", ");
                const QPainterPath::Element &dataElement1 = path.elementAt(++i);
                code.append(QString::number(dataElement1.x, 'f', 1) + ", " + QString::number(dataElement1.y, 'f', 1)  + ", ");
                const QPainterPath::Element &dataElement2 = path.elementAt(++i);
                code.append(QString::number(dataElement2.x, 'f', 1) + ", " + QString::number(dataElement2.y, 'f', 1)  + ");\n");
            }
            break;
            default:
            case QPainterPath::MoveToElement:
                code.append("    c.moveTo(" + QString::number(element.x, 'f', 1) + ", " + QString::number(element.y, 'f', 1) + ");\n");
            break;
        }
    }
    code.append("    c.closePath();\n");
    if (m_pen.style() != Qt::NoPen) {
        if (m_pen != m_activePen) {
            m_activePen = m_pen;
            m_elements.last().code.append(
                        "    c.strokeStyle = '" + toCssStyle(m_activePen.color()) + "';\n");
        }
        code.append("    c.stroke();\n");
    }
    if (m_brush.style() != Qt::NoBrush) {
        if (m_brush != m_activeBrush) {
            m_activeBrush = m_brush;
            m_elements.last().code.append(
                        "    c.fillStyle = '" + toCssStyle(m_activeBrush.color()) + "';\n");
        }
        code.append("    c.fill();\n");
    }
}

#include "codepaintdevice.moc"
