#include "PropertyQVariant.h"

#include "Extension.h"
#include "QtnProperty/Widget/Delegates/PropertyDelegateFactory.h"
#include "QtnProperty/Widget/Delegates/PropertyEditorAux.h"
#include "QtnProperty/Widget/Delegates/PropertyEditorHandler.h"

#include "CustomPropertyEditorDialog.h"

#include <QEvent>
#include <QKeyEvent>

class QtnPropertyQVariantEditBttnHandler: public QtnPropertyEditorHandler<QtnPropertyQVariantBase, QtnLineEditBttn>
{
public:
	QtnPropertyQVariantEditBttnHandler(QtnPropertyQVariantBase& property, QtnLineEditBttn& editor)
		: QtnPropertyEditorHandlerType(property, editor)
		, dialog(&editor)
		, revert(false)
		, is_object(false)
	{
		updateEditor();

		editor.lineEdit->installEventFilter(this);

		QObject::connect(editor.toolButton, &QToolButton::clicked,
		this, &QtnPropertyQVariantEditBttnHandler::onToolButtonClicked);

		QObject::connect(editor.lineEdit, &QLineEdit::editingFinished,
		this, &QtnPropertyQVariantEditBttnHandler::onEditingFinished);

		QObject::connect(&dialog, &CustomPropertyEditorDialog::apply,
						 this, &QtnPropertyQVariantEditBttnHandler::onApplyData);
	}

protected:
	virtual void updateEditor() override
	{
		auto edit = editor().lineEdit;
		edit->setReadOnly(!property().isEditableByUser());
		auto &value = property().value();
		if (QtnPropertyQVariant::variantIsObject(value.type()))
		{
			is_object = true;
			edit->setText(QString());
		} else
		{
			is_object = false;
			edit->setText(value.toString());
		}

		edit->setPlaceholderText(QtnPropertyQVariant::getPlaceholderStr(value.type()));
	}

	virtual bool eventFilter(QObject *obj, QEvent *event) override
	{
		if (event->type() == QEvent::KeyPress)
		{
			QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
			// revert all changes
			if (keyEvent->key() == Qt::Key_Escape)
			{
				revert = true;
			}
		}

		return QObject::eventFilter(obj, event);
	}

private:
	void onEditingFinished()
	{
		auto text = editor().lineEdit->text();
		if (!revert && (!is_object || !text.isEmpty()))
		{
			property().setValue(text);
			updateEditor();
		}

		revert = false;
	}

	void onToolButtonClicked(bool)
	{
		QVariant data;
		auto text = editor().lineEdit->text();

		if (!text.isEmpty() || !is_object)
		{
			data = text;
		} else
			data = property().value();

		revert = true;
		dialog.setReadOnly(!property().isEditableByUser());

		if (dialog.execute(property().name(), data))
			property().setValue(data);

		updateEditor();
	}

	void onApplyData(const QVariant &data)
	{
		property().setValue(data);
		updateEditor();
	}

	CustomPropertyEditorDialog dialog;
	bool revert;
	bool is_object;
};

QtnPropertyQVariantBase::QtnPropertyQVariantBase(QObject *parent)
	: QtnSinglePropertyBase<const QVariant &>(parent)
{
}

bool QtnPropertyQVariantBase::fromStrImpl(const QString &str)
{
	return setValue(str);
}

bool QtnPropertyQVariantBase::toStrImpl(QString &str) const
{
	str = value().toString();
	return true;
}

bool QtnPropertyQVariantBase::fromVariantImpl(const QVariant &var)
{
	return setValue(var);
}

bool QtnPropertyQVariantBase::toVariantImpl(QVariant &var) const
{
	var = value();
	return var.isValid();
}

QtnPropertyQVariantCallback::QtnPropertyQVariantCallback(QObject *object, const QMetaProperty &meta_property)
	: QtnSinglePropertyCallback<QtnPropertyQVariantBase>(nullptr)
{
	setCallbackValueGet([this, object, meta_property]() -> const QVariant &
	{
		value = meta_property.read(object);
		return value;
	});

	setCallbackValueSet([this, object, meta_property](const QVariant &value)
	{
		this->value = value;
		meta_property.write(object, value);
	});

	setCallbackValueAccepted([this](const QVariant &) -> bool
	{
		return isEditableByUser();
	});
}

QtnPropertyQVariant::QtnPropertyQVariant(QObject *parent)
	: QtnSinglePropertyValue<QtnPropertyQVariantBase>(parent)
{
}

void QtnPropertyQVariant::Register()
{
	qtnRegisterMetaPropertyFactory(QMetaType::QVariant,
	[](QObject *object, const QMetaProperty &metaProperty) -> QtnProperty *
	{
		return new QtnPropertyQVariantCallback(object, metaProperty);
	});

	QtnPropertyDelegateFactory::staticInstance()
		.registerDelegateDefault(&QtnPropertyQVariantBase::staticMetaObject,
								 &qtnCreateDelegate<QtnPropertyDelegateQVariant, QtnPropertyQVariantBase>,
								 "QVariant");
}

QString QtnPropertyQVariant::valueToString(const QVariant &value)
{
	return !variantIsObject(value.type()) ? value.toString() : QString();
}

bool QtnPropertyQVariant::variantIsObject(QVariant::Type type)
{
	switch (type)
	{
		case QVariant::Map:
		case QVariant::List:
			return true;

		default:
			break;
	}

	return false;
}

QString QtnPropertyQVariant::getPlaceholderStr(QVariant::Type type)
{
	switch (type)
	{
		case QVariant::Map:
			return tr("(Dictionary)");

		case QVariant::List:
			return tr("(List)");

		default:
			break;
	}

	return tr("(Empty)");
}

QtnPropertyDelegateQVariant::QtnPropertyDelegateQVariant(QtnPropertyQVariantBase &owner)
	: QtnPropertyDelegateTyped<QtnPropertyQVariantBase>(owner)
{

}

bool QtnPropertyDelegateQVariant::acceptKeyPressedForInplaceEditImpl(QKeyEvent *keyEvent) const
{
	if (QtnPropertyDelegateTyped<QtnPropertyQVariantBase>::acceptKeyPressedForInplaceEditImpl(keyEvent))
		return true;

	// accept any printable key
	return qtnAcceptForLineEdit(keyEvent);
}

QWidget *QtnPropertyDelegateQVariant::createValueEditorImpl(QWidget *parent, const QRect &rect, QtnInplaceInfo *inplaceInfo)
{
	auto editor = new QtnLineEditBttn(parent);
	editor->setGeometry(rect);

	new QtnPropertyQVariantEditBttnHandler(owner(), *editor);

	qtnInitLineEdit(editor->lineEdit, inplaceInfo);
	return editor;
}

bool QtnPropertyDelegateQVariant::propertyValueToStr(QString &strValue) const
{
	auto &value = owner().value();
	strValue = QtnPropertyQVariant::valueToString(value);
	if (strValue.isEmpty())
		strValue = QtnPropertyQVariant::getPlaceholderStr(value.type());

	return true;
}

void QtnPropertyDelegateQVariant::drawValueImpl(QStylePainter &painter, const QRect &rect,
												const QStyle::State &state, bool *needTooltip) const
{
	QPen oldPen = painter.pen();
	if (QtnPropertyQVariant::valueToString(owner().value()).isEmpty())
		painter.setPen(Qt::darkGray);

	Inherited::drawValueImpl(painter, rect, state, needTooltip);
	painter.setPen(oldPen);
}
