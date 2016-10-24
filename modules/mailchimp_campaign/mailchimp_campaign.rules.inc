<?php

/**
 * @file
 * Contains rules to create and send campaigns.
 */

/**
 * Implements hook_rules_data_info().
 */
function mailchimp_campaign_rules_data_info() {
  return array(
    'mailchimp_campaign_content' => array(
      'label' => t('Mailchimp Content'),
      'ui class' => 'RulesDataUIMailchimpCampaignContent',
      'token type' => 'rules_text',
    ),
  );
}

/**
 * Implements hook_rules_evaluator_info().
 */
function mailchimp_campaign_rules_evaluator_info() {
  return array(
    'mailchimp_campaign_token' => array(
      'class' => 'MailchimpCampaignTokenEvaluator',
      'type' => 'mailchimp_campaign_content',
      'weight' => 0,
    ),
  );
}

/**
 * Implements hook_rules_action_info().
 */
function mailchimp_campaign_rules_action_info() {
  return array(
    'mailchimp_campaign_action_create_campaign' => array(
      'label' => t('Create mailchimp campaign'),
      'named parameter' => TRUE,
      'group' => t('Mailchimp Campaign'),
      'parameter' => array(
        'title' => array(
          'type' => 'text',
          'label' => t('Title'),
          'description' => t('An internal name to use for this campaign. By default, the campaign subject will be used.'),
          'optional' => TRUE,
        ),
        'subject' => array(
          'type' => 'text',
          'label' => t('Subject'),
        ),
        'list_id' => array(
          'type' => 'text',
          'label' => t('List'),
          'description' => t('Select the list this campaign should be sent to.'),
          'restriction' => 'input',
          'ui class' => 'RulesDataUIMailchimpCampaignListId',
        ),
        'from_email' => array(
          'type' => 'text',
          'label' => t('From Email'),
          'description' => t('the From: email address for your campaign message.'),
          'default value' => variable_get('site_mail'),
        ),
        'from_name' => array(
          'type' => 'text',
          'label' => t('From Name'),
          'description' => t('the From: name for your campaign message (not an email address)'),
          'default value' => variable_get('site_name'),
          'optional' => TRUE,
        ),
        'template_id' => array(
          'type' => 'integer',
          'label' => t('Template'),
          'description' => t('Select a MailChimp user template to use. Due to a limitation in the API, only templates that do not contain repeating sections are available. If empty, the default template will be applied.'),
          'restriction' => 'input',
          'ui class' => 'RulesDataUIMailchimpCampaignTemplateId',
        ),
        'content' => array(
          'type' => 'mailchimp_campaign_content',
          'label' => t('Content sections'),
          'description' => t('The HTML content or, if a template is selected, the content for each section.'),
          'restriction' => 'input',
        ),
      ),
      'provides' => array(
        'campaign_id' => array(
          'label' => t('Campaign ID'),
          'type' => 'integer',
          'save' => TRUE,
        ),
      ),
      'callbacks' => array(
        'execute' => 'mailchimp_campaign_action_create_campaign',
      ),
    ),
    'mailchimp_campaign_action_send_campaign' => array(
      'label' => t('Send mailchimp campaign'),
      'group' => t('Mailchimp Campaign'),
      'parameter' => array(
        'campaign_id' => array(
          'type' => 'integer',
          'label' => t('Campaign id'),
          'description' => t('The campaign id for the campaign that should be sent.'),
        ),
      ),
      'callbacks' => array(
        'execute' => 'mailchimp_campaign_action_send_campaign',
      ),
    ),
  );
}

/**
 * Ajax callback to return the content sections form.
 */
function mailchimp_campaign_ajax_template_sections_form($form, $form_state) {
  return $form['parameter']['content']['settings']['content'];
}

/**
 * The action function to create a mailchimp campaign.
 */
function mailchimp_campaign_action_create_campaign($mailchimp_campaign_settings) {
  $recipients = new stdClass();
  $recipients->list_id = $mailchimp_campaign_settings['list_id'];

  $options = new stdClass();
  $options->title = !empty($mailchimp_campaign_settings['title']) ? $mailchimp_campaign_settings['title'] : $mailchimp_campaign_settings['subject'];
  $options->subject_line = $mailchimp_campaign_settings['subject'];
  $options->from_email = $mailchimp_campaign_settings['from_email'];
  $options->from_name = check_plain($mailchimp_campaign_settings['from_name']);
  $options->reply_to = check_plain($mailchimp_campaign_settings['from_name']);

  // Save the mailchimp campaign.
  $campaign_id = mailchimp_campaign_save_campaign($mailchimp_campaign_settings['content'], $recipients, $options, $mailchimp_campaign_settings['template_id']);

  // Clear campaign cache.
  mailchimp_campaign_get_campaigns(array($campaign_id), TRUE);

  return array('campaign_id' => $campaign_id);
}

/**
 * The action function to send a mailchimp campaign.
 */
function mailchimp_campaign_action_send_campaign($campaign_id) {
  if (!empty($campaign_id)) {
    // Load the campaign from the id.
    $campaign = mailchimp_campaign_load($campaign_id);
  }

  if (!empty($campaign)) {
    // Send the campaign.
    mailchimp_campaign_send_campaign($campaign);
    drupal_set_message(t('Campaign %name has been sent.', array('%name' => $campaign->label())));

    // Clear campaign cache.
    mailchimp_campaign_get_campaigns(array($campaign_id), TRUE);
  }
  else{
    drupal_set_message(t('The campaign could not be sent because the campaign or campaign id is invalid.'), array(), 'error');
  }
}

/**
 * Defines an input form for the List id.
 */
class RulesDataUIMailchimpCampaignListId extends RulesDataUI implements RulesDataDirectInputFormInterface {
  /**
   * Specifies the default input mode per data type.
   */
  public static function getDefaultMode() {
    return 'input';
  }

  /**
   * Constructs the direct input form.
   */
  public static function inputForm($name, $info, $settings, RulesPlugin $element) {
    $settings += array($name => isset($info['default value']) ? $info['default value'] : array('list_id' => NULL));
    $value = $settings[$name];

    module_load_include('inc', 'mailchimp_campaign', 'includes/mailchimp_campaign.admin');
    $form['list_id'] = array(
      '#type' => 'select',
      '#title' => t('Select'),
      '#options' => _mailchimp_campaign_build_option_list(mailchimp_get_lists()),
      '#default_value' => $value,
      '#required' => TRUE,
    );

    return $form;
  }

  /**
   * Build a render array for the campaign list selector.
   */
  public static function render($value) {
    module_load_include('inc', 'mailchimp_campaign', 'includes/mailchimp_campaign.admin');
    $lists = _mailchimp_campaign_build_option_list(mailchimp_get_lists());
    if (!empty($lists[$value])) {
      return array(
        'content' => array(
          '#markup' => $lists[$value],
        ),
      );
    }
  }
}

/**
 * Defines an input form for the Template id.
 */
class RulesDataUIMailchimpCampaignTemplateId extends RulesDataUI implements RulesDataDirectInputFormInterface {
  /**
   * Specifies the default input mode per data type.
   */
  public static function getDefaultMode() {
    return 'input';
  }

  /**
   * Constructs the direct input form.
   */
  public static function inputForm($name, $info, $settings, RulesPlugin $element) {
    $settings += array($name => isset($info['default value']) ? $info['default value'] : array('template_id' => NULL));
    $value = $settings[$name];

    module_load_include('inc', 'mailchimp_campaign', 'includes/mailchimp_campaign.admin');
    $form['template_id'] = array(
      '#type' => 'select',
      '#title' => t('Select'),
      '#options' => _mailchimp_campaign_build_option_list(mailchimp_campaign_list_templates()),
      '#default_value' => $value,
      '#required' => FALSE,
      '#ajax' => array(
        'wrapper' => 'edit-parameter-content-settings-content',
        'callback' => 'mailchimp_campaign_ajax_template_sections_form',
      ),
    );

    return $form;
  }

  /**
   * Render the configured value.
   */
  public static function render($value) {
    return array(
      'content' => array(
        '#markup' => !empty($value) ? $value : 'default',
      ),
    );
  }
}

/**
 * Defines an input form for the Content.
 */
class RulesDataUIMailchimpCampaignContent extends RulesDataUI implements RulesDataDirectInputFormInterface {
  /**
   * Specifies the default input mode per data type.
   */
  public static function getDefaultMode() {
    return 'input';
  }

  /**
   * Constructs the direct input form.
   */
  public static function inputForm($name, $info, $settings, RulesPlugin $element) {
    $settings += array($name => isset($info['default value']) ? $info['default value'] : array('content' => NULL));
    $value = $settings[$name];

    $form['content'] = array(
      '#type' => 'container',
      '#id' => 'edit-parameter-content-settings-content',
      '#tree' => TRUE,
    );

    if (isset($settings['template_id']) && !empty($settings['template_id'])) {
      $mc_template = mailchimp_campaign_get_template($settings['template_id']);
      if (!empty($mc_template->info->sections)) {
        foreach ($mc_template->info->sections as $section => $content) {
          // Set the default value and text format to either saved campaign values
          // or defaults coming from the MC template.
          $default_value = isset($value[$section]['value']) ? $value[$section]['value'] : $content;
          $format = 'mailchimp_campaign';
          $form['content'][$section] = array(
            '#type' => 'text_format',
            '#format' => $format,
            '#title' => drupal_ucfirst($section),
            '#default_value' => $default_value,
          );
        }
      }

      // If template was not available or there are no sections, add a default section.
      if (empty($mc_template->info->sections) || empty($section)) {
        $form['content']['html'] = array(
          '#type' => 'text_format',
          '#format' => 'mailchimp_campaign',
          '#title' => t('Content'),
          '#default_value' => isset($value['html']['value']) ? $value['html']['value'] : '',
          '#description' => t('The HTML content of the campaign.'),
        );
      }
      RulesDataInputEvaluator::attachForm($form, $settings, $info, $element->availableVariables());
    }
    return $form;
  }
  /**
   * Render the configured value.
   */
  public static function render($value) {
    $sections = array_keys($value);
    return array(
      'content' => array(
        '#markup' => implode(', ', $sections),
      ),
    );
  }
}


/**
 * A class implementing a rules input evaluator processing tokens.
 */
class MailchimpCampaignTokenEvaluator extends RulesDataInputEvaluator {

  /**
   * Prepares the evaluation.
   */
  public function prepare($values, $variables) {
    $needs_scan = NULL;
    foreach ($values as $value) {
      if (token_scan($value['value'])) {
        $needs_scan = TRUE;
      }
    }
    // Skip this evaluator if there are no tokens.
    $this->setting = $needs_scan;
  }

  /**
   * Performs the token replacement.
   *
   * We replace the tokens on our own as we cannot use token_replace(), because
   * token usually assumes that $data['node'] is a of type node, which doesn't
   * hold in general in our case.
   * So we properly map variable names to variable data types and then run the
   * replacement ourself.
   */
  public function evaluate($values, $options, RulesState $state) {
    $variables = $state->varInfo();
    $options += array('sanitize' => FALSE);

    // Replace the tokens in each section.
    foreach ($values as $section => $value) {
      $replacements[$section] = array();
      $data = array();
      foreach (token_scan($value['value']) as $var_name => $tokens) {
        $var_name = str_replace('-', '_', $var_name);
        if (isset($variables[$var_name]) && ($token_type = _rules_system_token_map_type($variables[$var_name]['type']))) {
          // We have to key $data with the type token uses for the variable.
          $data = rules_unwrap_data(array($token_type => $state->get($var_name)), array($token_type => $variables[$var_name]));
          $replacements[$section] += token_generate($token_type, $tokens, $data, $options);
        }
        else {
          $replacements[$section] += token_generate($var_name, $tokens, array(), $options);
        }
        // Remove tokens if no replacement value is found. As token_replace()
        // does if 'clear' is set.
        $replacements[$section] += array_fill_keys($tokens, '');
      }

      // Optionally clean the list of replacement values.
      if (!empty($options['callback']) && function_exists($options['callback'])) {
        $function = $options['callback'];
        $function($replacements[$section], $data, $options);
      }

      // Actually apply the replacements.
      $token_keys = array_keys($replacements[$section]);
      $token_values = array_values($replacements[$section]);
      $values[$section]['value'] = str_replace($token_keys, $token_values, $values[$section]['value']);
    }
    return $values;
  }

  /**
   * Create documentation about the available replacement patterns.
   *
   * @param array $variables
   *   Array with the available variables.
   *
   * @return array
   *   Renderable array with the replacement pattern documentation.
   */
  public static function help($variables) {
    $render = array(
      '#type' => 'fieldset',
      '#title' => t('Replacement patterns'),
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
      '#description' => t('Note that token replacements containing chained objects – such as [node:author:uid] – are not listed here, but are still available. The <em>data selection</em> input mode may help you find more complex replacement patterns. See <a href="@url">the online documentation</a> for more information about complex replacement patterns.',
        array('@url' => rules_external_help('chained-tokens'))),
    );

    $token_info = token_info();
    $token_types = array();
    foreach ($variables as $name => $info) {
      $token_types[$name] = _rules_system_token_map_type($info['type']);
    }

    foreach ($token_types as $name => $token_type) {
      if (isset($token_info['types'][$token_type])) {
        $render[$name] = array(
          '#theme' => 'table',
          '#header' => array(t('Token'), t('Label'), t('Description')),
          '#prefix' => '<h3>' . t('Replacement patterns for %label', array('%label' => $variables[$name]['label'])) . '</h3>',
        );
        foreach ($token_info['tokens'][$token_type] as $token => $info) {
          $token = '[' . str_replace('_', '-', $name) . ':' . $token . ']';
          $render[$name]['#rows'][$token] = array(
            check_plain($token),
            check_plain($info['name']),
            check_plain($info['description']),
          );
        }
      }
    }
    return $render;
  }
}
