#include "Geode/cocos/cocoa/CCArray.h"
#include "Geode/utils/cocos.hpp"
#include <Geode/Geode.hpp>
#include <Geode/binding/EditorUI.hpp>
#include <Geode/binding/GameObject.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <razoom.save_level_data_api/include/SaveLevelDataApi.hpp>

using namespace geode::prelude;

class $modify(MyEditorUI, EditorUI) {

	struct Fields {
		Ref<GameObject> m_defaultObject;
	};

    bool init(LevelEditorLayer* editorLayer) {
		if (!EditorUI::init(editorLayer)) return false;
		auto fields = m_fields.self();

		geode::Result<matjson::Value> result = SaveLevelDataAPI::getSavedValue(
            editorLayer->m_level,
            "default-object",
            false,
            true
        );

		if (result) {
			auto objects = editorLayer->createObjectsFromString(result.unwrap().asString().unwrap(), false, false);
			fields->m_defaultObject = static_cast<GameObject*>(objects->firstObject());
			deleteObject(fields->m_defaultObject, true);
		}
		else {
			fields->m_defaultObject = GameObject::createWithKey(207);
			// if GD doesn't manage these for some reason, die.
			fields->m_defaultObject->m_baseColor = new GJSpriteColor();
			fields->m_defaultObject->m_detailColor = new GJSpriteColor();
		}

		schedule(schedule_selector(MyEditorUI::updateObjectColors));
		return true;
	}

    void updateButtons() {
		EditorUI::updateButtons();
		m_editObjectBtn->setOpacity(255);
		m_editObjectBtn->setColor({255, 255, 255});
		m_editObjectBtn->m_animationEnabled = true;
	}

    void editObject(cocos2d::CCObject* sender) {
		if (!m_selectedObject && (!m_selectedObjects || m_selectedObjects->count() == 0)) {
			auto fields = m_fields.self();
			auto customizeObjectLayer = CustomizeObjectLayer::create(fields->m_defaultObject, nullptr);
			customizeObjectLayer->show();
		}
		else {
			EditorUI::editObject(sender);
		}
	}

    GameObject* createObject(int p0, cocos2d::CCPoint p1) {
		auto ret = EditorUI::createObject(p0, p1);

		auto defaultObject = m_fields->m_defaultObject;

		int baseColorID = defaultObject->m_baseColor->m_colorID;

		if (baseColorID == 1012 && !ret->m_detailColor) {
			baseColorID = 1011;
        }

		if (auto baseColor = ret->m_baseColor) {
			baseColor->m_colorID = baseColorID;
			baseColor->m_hsv = defaultObject->m_baseColor->m_hsv;
		}
		if (auto detailColor = ret->m_detailColor) {
			detailColor->m_colorID = defaultObject->m_detailColor->m_colorID;
			detailColor->m_hsv = defaultObject->m_detailColor->m_hsv;
		}

		return ret;
	}

	std::pair<ccColor3B, bool> getActiveColor(int colorID) {
		for (ColorActionSprite* action : m_editorLayer->m_effectManager->m_colorActionSpriteVector) {
			if (!action) continue;
			if (action->m_colorID != colorID || action->m_colorID <= 0) continue;

			ccColor3B color = action->m_color;

			if (colorID == 1005) color = GameManager::get()->colorForIdx(GameManager::get()->m_playerColor.value());
			if (colorID == 1006) color = GameManager::get()->colorForIdx(GameManager::get()->m_playerColor2.value());

			if (colorID == 1010) color = {0, 0, 0};
			if (colorID == 1011) color = {255, 255, 255};

			for (PulseEffectAction& pulse : m_editorLayer->m_effectManager->m_pulseEffectVector) {
				if (pulse.m_targetGroupID == action->m_colorID) {
					color = m_editorLayer->m_effectManager->colorForPulseEffect(color, &pulse);
				}
			}

			bool blending = false;
			if (auto colorAction = action->m_colorAction) {
				blending = colorAction->m_blending;
			}

			return {color, blending};
		}
		return {{255, 255, 255}, false};
	}


	bool isColorable(GameObject* object) {
		const auto type = object->m_objectType;
		const auto id = object->m_objectID;

		static const std::unordered_set<int> allowedIDs = {
			3027, 1594
		};

		static const std::unordered_set<int> disallowedIDs = {
			2069, 3645, 3032, 2016, 1816, 3642, 3643, 2064
		};

		static const std::unordered_set<GameObjectType> disallowedTypes = {
			GameObjectType::YellowJumpPad,
			GameObjectType::PinkJumpPad,
			GameObjectType::RedJumpPad,
			GameObjectType::GravityPad,
			GameObjectType::SpiderPad,
			GameObjectType::SpiderOrb,
			GameObjectType::YellowJumpRing,
			GameObjectType::PinkJumpRing,
			GameObjectType::RedJumpRing,
			GameObjectType::GravityRing,
			GameObjectType::GreenRing,
			GameObjectType::DropRing,
			GameObjectType::DashRing,
			GameObjectType::GravityDashRing,
			GameObjectType::NormalGravityPortal,
			GameObjectType::InverseGravityPortal,
			GameObjectType::GravityTogglePortal,
			GameObjectType::CubePortal,
			GameObjectType::ShipPortal,
			GameObjectType::UfoPortal,
			GameObjectType::BallPortal,
			GameObjectType::WavePortal,
			GameObjectType::RobotPortal,
			GameObjectType::SpiderPortal,
			GameObjectType::SwingPortal,
			GameObjectType::NormalMirrorPortal,
			GameObjectType::InverseMirrorPortal,
			GameObjectType::MiniSizePortal,
			GameObjectType::RegularSizePortal,
			GameObjectType::DualPortal,
			GameObjectType::SoloPortal,
			GameObjectType::TeleportPortal,
			GameObjectType::SecretCoin,
			GameObjectType::UserCoin
		};

		if (allowedIDs.contains(id)) return true;
		if (disallowedIDs.contains(id)) return false;
		if (object->isTrigger() || object->isSpeedObject()) return false;
		if (disallowedTypes.contains(type)) return false;

		return true;
	}

	void updateObjectColors(float dt) {
		auto defaultObject = m_fields->m_defaultObject;

		for (auto bar : CCArrayExt<EditButtonBar*>(m_createButtonBars)) {
			if (!bar->isVisible()) continue;
			for (auto btn : CCArrayExt<CCMenuItem*>(bar->m_buttonArray)) {
				if (auto parent = btn->getParent()) {
					// prevent crashes if button and/or menu are for some reason not part of a ButtonPage (???)
					if (auto parent2 = typeinfo_cast<ButtonPage*>(parent->getParent())) {
						if (!parent2->isVisible()) continue;
					}
				}
				if (auto btnSprite = btn->getChildByType<ButtonSprite>(0)) {
					if (auto gameObject = btnSprite->getChildByType<GameObject>(0)) {
						if (!isColorable(gameObject)) continue;
						
						if (auto baseColor = gameObject->m_baseColor) {
							baseColor->m_colorID = defaultObject->m_baseColor->m_colorID;
							baseColor->m_hsv = defaultObject->m_baseColor->m_hsv;

							auto color = ccColor3B{255, 255, 255};
							bool blending = false;
							gameObject->updateHSVState();

							if (defaultObject->m_baseColor->m_colorID != 0) {
								auto pair = getActiveColor(baseColor->m_colorID);
								color = pair.first;
								blending = pair.second;
							}

							auto blend = blending 
								? ccBlendFunc{GL_SRC_ALPHA, GL_ONE} 
								: ccBlendFunc{GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA};

							if (typeinfo_cast<EnhancedGameObject*>(gameObject)) {
								for (auto child : gameObject->getChildrenExt()) {
									auto sprite = typeinfo_cast<CCSprite*>(child);
									if (sprite) sprite->setBlendFunc(blend);
								}
							}
							gameObject->setBlendFunc(blend);

							if (baseColor->m_usesHSV) color = GameToolbox::transformColor(color, baseColor->m_hsv);
							gameObject->updateMainColor(color);
						}
						if (auto detailColor = gameObject->m_detailColor) {
							detailColor->m_colorID = defaultObject->m_detailColor->m_colorID;
							detailColor->m_hsv = defaultObject->m_detailColor->m_hsv;

							auto color = ccColor3B{200, 200, 255};
							bool blending = false;
							gameObject->updateHSVState();

							if (defaultObject->m_detailColor->m_colorID != 0) {
								auto pair = getActiveColor(detailColor->m_colorID);
								color = pair.first;
								blending = pair.second;
							}

							auto blend = blending 
								? ccBlendFunc{GL_SRC_ALPHA, GL_ONE} 
								: ccBlendFunc{GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA};

							std::function<void(CCNode*)> applyBlend = [&](CCNode* node) {
								for (auto child : node->getChildrenExt()) {
									auto sprite = typeinfo_cast<CCSprite*>(child);
									if (sprite) sprite->setBlendFunc(blend);
									applyBlend(child);
								}
							};
							if (typeinfo_cast<EnhancedGameObject*>(gameObject)) {
								for (auto child : gameObject->getChildrenExt()) {
									applyBlend(child);
								}
							}
							applyBlend(gameObject);

							if (detailColor->m_usesHSV) color = GameToolbox::transformColor(color, detailColor->m_hsv);
							gameObject->updateSecondaryColor(color);
						}
					}
				}
			}
		}
	}
};

class $modify(MyEditorPauseLayer, EditorPauseLayer) {

    void saveLevel() {
		auto editorUI = static_cast<MyEditorUI*>(EditorUI::get());

		SaveLevelDataAPI::setSavedValue(
            m_editorLayer->m_level,
            "default-object",
            editorUI->m_fields->m_defaultObject->getSaveString(m_editorLayer),
            false,
            true
        );

		EditorPauseLayer::saveLevel();
	}
};