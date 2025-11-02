#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <razoom.save_level_data_api/include/SaveLevelDataApi.hpp>

using namespace geode::prelude;

class Group : public CCNode {};

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
			auto objects = editorLayer->createObjectsFromString(result.unwrap().asString().unwrap(), true, true);
			fields->m_defaultObject = static_cast<GameObject*>(objects->firstObject());
			deleteObject(fields->m_defaultObject, true);
		}
		else {
			fields->m_defaultObject = GameObject::createWithKey(207);
			// if GD doesn't manage these for some reason, die.
			fields->m_defaultObject->m_baseColor = new GJSpriteColor();
			fields->m_defaultObject->m_detailColor = new GJSpriteColor();
		}

		fields->m_defaultObject->m_baseColor->m_defaultColorID = 0;
		fields->m_defaultObject->m_detailColor->m_defaultColorID = 0;

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

	struct ColorData {
		ccColor3B color;
		bool blending;
		GLubyte opacity;
	};

	ColorData getActiveColor(int colorID) {
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
			GLubyte opacity = 255;
			if (auto colorAction = action->m_colorAction) {
				blending = colorAction->m_blending;
				opacity = colorAction->m_currentOpacity * 255;
			}

			return {color, blending, opacity};
		}
		return {{255, 255, 255}, false, 255};
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

	void updateButton(CCNode* btn, GameObject* defaultObject) {
		if (auto btnSprite = btn->getChildByType<ButtonSprite>(0)) {
			for (auto child : btnSprite->getChildrenExt()) {
				if (auto gameObject = typeinfo_cast<GameObject*>(child)) {
					if (!isColorable(gameObject)) return;
					
					if (auto baseColor = gameObject->m_baseColor) {
						baseColor->m_colorID = defaultObject->m_baseColor->m_colorID;
						baseColor->m_hsv = defaultObject->m_baseColor->m_hsv;

						auto color = ccColor3B{255, 255, 255};
						bool blending = false;
						gameObject->updateHSVState();

						if (defaultObject->m_baseColor->m_colorID != 0) {
							auto colorData = getActiveColor(baseColor->m_colorID);
							color = colorData.color;
							blending = colorData.blending;
							baseColor->m_opacity = colorData.opacity;
						}

						auto blend = blending 
							? ccBlendFunc{GL_SRC_ALPHA, GL_ONE} 
							: ccBlendFunc{GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA};

						if (auto anim = typeinfo_cast<AnimatedGameObject*>(gameObject)) {
							if (auto animSpr = anim->m_animatedSprite) {
								if (auto paSpr = animSpr->m_paSprite) {
									for (auto child : paSpr->getChildrenExt()) {
										if (child == anim->m_eyeSpritePart && !anim->m_childSprite) continue;
										auto spr = static_cast<CCSprite*>(child);
										spr->setBlendFunc(blend);
									}
								}
							}
						}
						else if (typeinfo_cast<EnhancedGameObject*>(gameObject) || gameObject->m_hasCustomChild) {
							for (auto child : gameObject->getChildrenExt()) {
								if (child == gameObject->m_colorSprite) continue;
								if (auto spr = typeinfo_cast<CCSprite*>(child)) {
									spr->setBlendFunc(blend);
								}
							}
						}

						const auto& id = gameObject->m_objectID;
						if (id == 1701 || id == 1702 || id == 1703) {
							for (auto child : gameObject->getChildrenExt()) {
								if (child->getChildrenCount() == 0) {
									if (auto spr = typeinfo_cast<CCSprite*>(child)) {
										spr->setBlendFunc(blend);
									}
								}
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
							auto colorData = getActiveColor(detailColor->m_colorID);
							color = colorData.color;
							blending = colorData.blending;
							detailColor->m_opacity = colorData.opacity;
						}

						auto blend = blending 
							? ccBlendFunc{GL_SRC_ALPHA, GL_ONE} 
							: ccBlendFunc{GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA};

						std::function<void(CCNode*)> applyBlend = [&](CCNode* node) {
							for (auto child : node->getChildrenExt()) {
								if (auto spr = typeinfo_cast<CCSprite*>(child)) {
									spr->setBlendFunc(blend);
									applyBlend(spr);
								}
							}
						};
						
						if (auto anim = typeinfo_cast<AnimatedGameObject*>(gameObject)) {
							if (anim->m_childSprite) {
								anim->m_childSprite->setBlendFunc(blend);
							}
							else {
								if (auto eye = anim->m_eyeSpritePart) {
									eye->setBlendFunc(blend);
								}
							}
						}
						else if (typeinfo_cast<EnhancedGameObject*>(gameObject)) {
							for (auto child : gameObject->getChildrenExt()) {
								applyBlend(child);
							}
						}
						else {
							const auto& id = gameObject->m_objectID;
							if (id == 1701 || id == 1702 || id == 1703) {
								for (auto child : gameObject->getChildrenExt()) {
									if (child->getChildrenCount() > 0) {
										applyBlend(child);
										if (auto spr = typeinfo_cast<CCSprite*>(child)) {
											spr->setBlendFunc(blend);
										}
									}
								}
							}
							else {
								applyBlend(gameObject);
							}
						}

						if (auto spr = gameObject->m_colorSprite) {
							spr->setBlendFunc(blend);
						}

						if (detailColor->m_usesHSV) color = GameToolbox::transformColor(color, detailColor->m_hsv);
						gameObject->updateSecondaryColor(color);
					}
				}
			}
		}
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
				updateButton(btn, defaultObject);
			}
			for (auto buttonPage : CCArrayExt<ButtonPage*>(bar->m_pagesArray)) {
				if (auto menu = buttonPage->getChildByType<CCMenu>(0)) {
					if (auto group = menu->getChildByType<Group>(0)) {
						if (!group->isVisible()) continue;
						if (auto innerMenu = group->getChildByType<CCMenu>(1)) {
							for (auto btn : innerMenu->getChildrenExt()) {
								updateButton(btn, defaultObject);
							}
						}
					}
				}
			}
		}
		if (auto pinned = getChildByID("razoom.object_groups/pinned-groups")) {
			for (auto group : pinned->getChildrenExt()) {
				if (!group->isVisible()) continue;
				if (auto innerMenu = group->getChildByType<CCMenu>(1)) {
					for (auto btn : innerMenu->getChildrenExt()) {
						updateButton(btn, defaultObject);
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
            std::string_view(editorUI->m_fields->m_defaultObject->getSaveString(m_editorLayer)),
            false,
            true
        );

		EditorPauseLayer::saveLevel();
	}
};
