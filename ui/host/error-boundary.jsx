module.exports = function createErrorBoundary(React, RN, reportError) {
  return class ExtensionErrorBoundary extends React.Component {
    constructor(props) {
      super(props);
      this.state = { error: null };
    }
    static getDerivedStateFromError(error) {
      return { error };
    }
    componentDidCatch(error) {
      reportError(this.props.owner, error);
    }
    render() {
      return this.state.error ? (
        <RN.Text style={{ padding: 24 }}>
          Extension unavailable: {this.props.owner}
        </RN.Text>
      ) : (
        this.props.children
      );
    }
  };
};
