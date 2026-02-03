/* @refresh reload */
import { render } from 'solid-js/web'
import { Router, Route } from '@solidjs/router'
import './index.css'
import App from './App'
import Status from './Status'
import DoorControl from './DoorControl'
import WaterControl from './WaterControl'
import ManualControls from './ManualControls'
import Scenarios from './Scenarios'
import Recordings from './Recordings'
import Temperature from './Temperature'
import Settings from './Settings'

const root = document.getElementById('root')

render(() => (
  <Router root={App}>
    <Route path="/" component={Status} />
    <Route path="/status" component={Status} />
    <Route path="/door" component={DoorControl} />
    <Route path="/water" component={WaterControl} />
    <Route path="/controls" component={ManualControls} />
    <Route path="/scenarios" component={Scenarios} />
    <Route path="/recordings" component={Recordings} />
    <Route path="/temperature" component={Temperature} />
    <Route path="/settings" component={Settings} />
  </Router>
), root!)
