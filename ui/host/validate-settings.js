// Match the broker's setting types and numeric bounds before publishing a patch.
module.exports = function validateSettings(schema, values) {
  if (!values || Array.isArray(values) || typeof values !== 'object')
    throw Error('Invalid settings request');
  Object.keys(values).forEach((name) => {
    if (!/^[a-zA-Z0-9_-][a-zA-Z0-9_.-]{0,126}$/.test(name))
      throw Error('Invalid setting name');
    const rule = schema[name] || {},
      value = values[name],
      type = rule.type;
    if (
      ((type === 'boolean' || type === 'string') && typeof value !== type) ||
      ((type === 'number' || type === 'integer') &&
        (typeof value !== 'number' || !Number.isFinite(value)))
    )
      throw Error('Setting type mismatch');
    if (type === 'integer' && !Number.isInteger(value))
      throw Error('Setting requires an integer');
    if (
      typeof value === 'number' &&
      ((rule.min !== undefined && value < rule.min) ||
        (rule.max !== undefined && value > rule.max))
    )
      throw Error('Setting out of range');
  });
};
